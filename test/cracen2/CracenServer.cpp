#include <memory>
#include <typeinfo>
#include <mutex>
#include <set>

#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/AsioDatagram.hpp"

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/util/AtomicQueue.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::backend;

constexpr std::array<unsigned int, 3> participantsPerRole = {{2, 2, 2}};
// constexpr std::array<unsigned int, 3> participantsPerRole = {{1, 1, 1}};

template <class SocketImplementation>
struct CracenServerTest {

	using Communicator = network::Communicator<SocketImplementation, backend::ServerTagList<typename SocketImplementation::Endpoint>>;
	using Endpoint = typename Communicator::Endpoint;
	using Edge = std::pair<RoleId, RoleId>;

	TestSuite testSuite;

	const std::vector<Edge> edges;
	std::multiset<Edge> totalEdges;
	std::mutex totalEdgesLock;

	CracenServer<SocketImplementation> server;
	std::vector<JoiningThread> participants;

	void participantFunction(unsigned int role) {
		bool embodied = false;
		Communicator communicator;
		communicator.bind();
		communicator.sendTo(Register(), server.getEndpoint());
		bool contextReady = false;

		auto contextCreationVisitor = Communicator::make_visitor(
			[this, &communicator, role](RoleGraphRequest, Endpoint from){
				// New context on the Server
 				std::cout << "Client(" << role << "): Received RoleGraphRequest" << std::endl;

				for(const auto edge : edges) {
					std::cout << "Client(" << role << "): Send " << edge.first <<  " -> " << edge.second << std::endl;
					communicator.sendTo(AddRoleConnection { edge.first, edge.second }, from);
				}
				std::cout << "Send roles complete." << std::endl;
				communicator.sendTo(RolesComplete(), from);
			},
			[/*this, role*/](AddRoleConnection, Endpoint){
				// New context on the Server
 				// std::cout << "Client(" << role << "): Received addRoleConnectionAck " << addRoleConnectionAck.from << "->" << addRoleConnectionAck.to << std::endl;
			},
			[&contextReady, role](RolesComplete, Endpoint){
				// Ready to use context on the server
				std::cout << "Client(" << role << "): Received RolesCompleteAck" << std::endl;
				contextReady = true;
			}
		);

		auto contextAliveVisitor = Communicator::make_visitor(
			[role, &communicator, this](Embody<Endpoint> embody, Endpoint from){
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
				std::cout << "totalEdges left = " << totalEdges.size() << std::endl;
				if(totalEdges.size() == 0) {
 					std::cout << "All edges embodied! Success." << std::endl;
					communicator.sendTo(Disembody<Endpoint> {communicator.getLocalEndpoint() }, from);
				}
			},
			[/*role, &communicator, this*/](Announce<Endpoint>, Endpoint){},
			[/*role,*/ &embodied, &contextReady, &communicator, this](Disembody<Endpoint>, Endpoint from){
				testSuite.test(totalEdges.size() == 0, "One participant disembodied, before all connections were embodied.");
				std::stringstream s;
//  				s << "Client(" << role << "): received disembody" << disembody.endpoint << " me: " << communicator.getLocalEndpoint() << std::endl;
				std::cout << s.rdbuf();
				if(embodied) {
					// Remote end is closing. Shutting down myself
					communicator.sendTo(Disembody<Endpoint> {communicator.getLocalEndpoint()}, from);
					embodied = false;
				}
				contextReady = false;
			}
		);

		do {
			try {
				communicator.receive(contextCreationVisitor);
			} catch(const std::exception& e) {
				std::cout << "CracenServerTest(participantFunction) threw an exception: " << e.what() << std::endl;
				break;
			}
		} while(!contextReady);


 		std::cout << "Client(" << role << "): going into state running." << std::endl;
		std::async(
			std::launch::async,
			 [&communicator, &role, from = server.getEndpoint()](){
				communicator.sendTo(Embody<Endpoint>{communicator.getLocalEndpoint(), role}, from);
			}
		);
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
		server()
	{
		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int id = 0; id < participantsPerRole[role]; id++) {
				participants.emplace_back(
					"CracenServerTest::participant[" + std::to_string(role) + "]",
					&CracenServerTest::participantFunction,
					this,
					role % 3
				);
			}
		}

	}

	~CracenServerTest() {
// 		std::cout << "Destructor" << std::endl;
		participants.clear();
// 		std::cout << "Participants finished" << std::endl;
		server.stop();
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
	CracenServerTest<BoostMpiSocket> mpiServerTest(testSuite);
}
