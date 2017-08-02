#include <memory>

#include "cracen2/util/Test.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/sockets/Asio.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::network;
using namespace cracen2::sockets;
using namespace cracen2::backend;

int main(int argc, char* argv[]) {
	TestSuite testSuite("CracenServer");

	std::vector<std::pair<RoleId, RoleId>> edges;
	edges.push_back(std::make_pair(0,1));
	edges.push_back(std::make_pair(1,2));

	using SocketImplementation = AsioStreamingSocket;
	using Communicator = Communicator<SocketImplementation, backend::ServerTagList>;
	using Visitor = typename Communicator::Visitor;

	const SocketImplementation::Endpoint serverEndpoint(
		boost::asio::ip::address::from_string("127.0.0.1"),
		39391
	);


	std::unique_ptr<CracenServer<SocketImplementation>> server;
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

	Communicator communicator;
	communicator.connect(serverEndpoint);
	communicator.send(Register());

	bool contextReady = false;

	Visitor visitor(
		[&edges, &communicator](RoleGraphRequest roleGraphRequest){
			// New context on the Server
			std::cout << "Received RoleGraphRequest" << std::endl;
			for(const auto edge : edges) {
				communicator.send(AddRoleConnection { edge.first, edge.second });
			}
			communicator.send(RolesComplete());
		},
		[&edges](AddRoleConnection addRoleConnectionAck){
			// New context on the Server
			std::cout << "Received addRoleConnectionAck " << addRoleConnectionAck.from << "->" << addRoleConnectionAck.to << std::endl;
		},
		[&contextReady](RolesComplete rolesComplete){
			// Ready to use context on the server
			std::cout << "Received RolesCompleteAck" << std::endl;
			contextReady = true;
		}
	);

	while(!contextReady) {
		communicator.receive(visitor);
	}
	std::cout << "Finished Graph creation" << std::endl;

	server->stop();

}
