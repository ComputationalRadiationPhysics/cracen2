#include <memory>
#include <typeinfo>
#include <mutex>
#include <set>

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/sockets/Asio.hpp"
#include "cracen2/util/AtomicQueue.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::backend;

constexpr std::array<unsigned int, 3> participantsPerRole = {{2, 2, 2}};

constexpr std::uint16_t serverPort = 39391;

template <class SocketImplementation>
struct CracenServerTest {

	using Communicator = network::Communicator<SocketImplementation, backend::ServerTagList<typename SocketImplementation::Endpoint>>;
	using Endpoint = typename Communicator::Endpoint;
	using Visitor = typename Communicator::Visitor;
	using Edge = std::pair<RoleId, RoleId>;

	TestSuite testSuite;

	const std::vector<Edge> edges;
	std::multiset<Edge> totalEdges;
	std::mutex totalEdgesLock;

	const Endpoint serverEndpoint;
	std::unique_ptr<CracenServer<SocketImplementation>> server;
	std::vector<JoiningThread> participants;

	void participantFunction(unsigned int role) {
		bool embodied = false;
		Communicator communicator;
		communicator.connect(serverEndpoint);

// 		std::cout << "Client(" << role << "): Send register" << std::endl;
		communicator.send(Register());

		bool contextReady = false;

		Visitor contextCreationVisitor(
			[this, &communicator, role](RoleGraphRequest){
				// New context on the Server
// 				std::cout << "Client(" << role << "): Received RoleGraphRequest" << std::endl;

				for(const auto edge : edges) {
					communicator.send(AddRoleConnection { edge.first, edge.second });
				}

				communicator.send(RolesComplete());
			},
			[this, role](AddRoleConnection){
				// New context on the Server
 				// std::cout << "Client(" << role << "): Received addRoleConnectionAck " << addRoleConnectionAck.from << "->" << addRoleConnectionAck.to << std::endl;
			},
			[&contextReady, role](RolesComplete){
				// Ready to use context on the server
// 				std::cout << "Client(" << role << "): Received RolesCompleteAck" << std::endl;
				contextReady = true;
			}
		);

		Visitor contextAliveVisitor(
			[role, &communicator, this](Embody<Endpoint> embody){
				std::stringstream s;
//  				s << "Client(" << role << "): received embody " << embody.endpoint << " -> " << embody.roleId << std::endl;
				std::cout << s.rdbuf();

				std::unique_lock<std::mutex> lock(totalEdgesLock);
				auto it = totalEdges.find(std::make_pair(role, embody.roleId));
				auto it2 = totalEdges.find(std::make_pair(embody.roleId, role));
				if(it != totalEdges.end()) {
					totalEdges.erase(it);
				} else if(it2 != totalEdges.end()) {
					totalEdges.erase(it2);
				} else {
					testSuite.test(
						false,
						std::string("There has been an embody message, that was too much. ") +
						std::to_string(role) + std::string(" -> ") + std::to_string(embody.roleId));
				}

				if(totalEdges.size() == 0) {
// 					std::cout << "All edges embodied!!!11!!elf" << std::endl;
					communicator.send(Disembody<Endpoint> {communicator.getLocalEndpoint() });
				}
			},
			[role, &communicator, this](Announce<Endpoint>){},
			[role, &embodied, &contextReady, &communicator, this](Disembody<Endpoint>){
				testSuite.test(totalEdges.size() == 0, "One participant disembodied, before all connections were embodied.");
				std::stringstream s;
//  				s << "Client(" << role << "): received disembody" << disembody.endpoint << " me: " << communicator.getLocalEndpoint() << std::endl;
				std::cout << s.rdbuf();
				if(embodied) {
					// Remote end is closing. Shutting down myself
					communicator.send(Disembody<Endpoint> {communicator.getLocalEndpoint()});
					embodied = false;
				}
				contextReady = false;
			}
		);

		do {
			communicator.receive(contextCreationVisitor);
		} while(!contextReady);

// 		std::cout << "Client(" << role << "): going into state running." << std::endl;
		communicator.send(Embody<Endpoint>{communicator.getLocalEndpoint(), role});
		embodied = true;

		do {
			communicator.receive(contextAliveVisitor);
		} while(contextReady);
//  		std::cout << "Client(" << role << "): Finished execution" << std::endl;
	};

	CracenServerTest(TestSuite& parent)  :
		testSuite(
			std::string("Implementation test for ")+demangle(typeid(SocketImplementation).name()),
			std::cout,
			&parent
		),
		edges { std::make_pair(0,1), std::make_pair(1,2) },
		totalEdges{ calculateTotalEdges() },
		serverEndpoint(
			boost::asio::ip::address::from_string("127.0.0.1"),
			serverPort
		)
	{
		for(int i = 0; i < 10; i++) {
			try {
				server = std::unique_ptr<CracenServer<SocketImplementation>>(
					new CracenServer<SocketImplementation>(serverEndpoint.port())
				);
				break;
			} catch(const std::exception&) {

			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int id = 0; id < participantsPerRole[role]; id++) {
				participants.push_back(
					JoiningThread(
						&CracenServerTest::participantFunction,
						this,
						role % 3
					)
				);
			}
		}

	}

	~CracenServerTest() {
// 		std::cout << "Destructor" << std::endl;
		participants.clear();
// 		std::cout << "Participants finished" << std::endl;
		server->stop();
	}

	std::multiset<Edge> calculateTotalEdges() const {
		std::multiset<Edge> result;
		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int source = 0; source < participantsPerRole[role]; source++) {
				for(const auto& metaEdge : edges) {
					if(metaEdge.first == role) {
						// Outgoing Edge
						for(unsigned int destination = 0; destination < participantsPerRole[metaEdge.second]; destination++) {
							result.insert(std::make_pair(metaEdge.first, metaEdge.second));
						}
					}
				}
			}
		}

		return result;
	}
}; // End of class CracenServerTest


int main() {
	TestSuite testSuite("Cracen Server Test");

	//CracenServerTest<AsioStreamingSocket> tcpServerTest(testSuite);
	CracenServerTest<AsioDatagramSocket> udpServerTest(testSuite);

}
