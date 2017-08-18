#include <memory>
#include <typeinfo>
#include <mutex>
#include <set>

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/CracenClient.hpp"
#include "cracen2/send_policies/broadcast.hpp"
#include "cracen2/sockets/Asio.hpp"
#include "cracen2/util/AtomicQueue.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::backend;

using DataTagList = std::tuple<int>;

constexpr std::array<
	std::pair<backend::RoleId, backend::RoleId>,
	3
> roleGraph {{
	std::make_pair(0, 1),
	std::make_pair(1, 2),
	std::make_pair(2, 0)
}};

constexpr std::array<unsigned int, 3> participantsPerRole = {{1, 1, 1}};

constexpr std::uint16_t serverPort = 39391;

template <class SocketImplementation>
struct CracenClientTest {

	using Endpoint = typename SocketImplementation::Endpoint;
	using CracenClient = cracen2::CracenClient<SocketImplementation, DataTagList>;

	TestSuite testSuite;

	cracen2::CracenServer<SocketImplementation> server;
	std::vector<
		std::unique_ptr<
			CracenClient
		>
	> clients;

	CracenClientTest(TestSuite& parent)  :
		testSuite(
			std::string("Implementation test for ")+demangle(typeid(SocketImplementation).name()),
			std::cout,
			&parent
		),
		server(
			serverPort
		)
	{
		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int id = 0; id < participantsPerRole[role]; id++) {
				clients.push_back(
					std::unique_ptr<CracenClient>(
						new CracenClient
						(
							Endpoint(
								boost::asio::ip::address::from_string("127.0.0.1"),
								serverPort
							),
							role,
							roleGraph
						)
					)
				);
			}
		}



		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		std::cout << "All Clients running." << std::endl;
// 		server.printStatus();
// 		for(auto& clientPtr : clients) {
// 			clientPtr->printStatus();
// 		}

		std::async(std::launch::async, [this](){
			int id = 0;
			for(auto& clientPtr : clients) {
				try {
					clientPtr->send(id, send_policies::broadcast_any());
					id++;
				} catch(const std::exception& e) {
					std::cerr << "Sending failed: " << e.what() << std::endl;
					testSuite.test(false, "Error during sending.");
				}
			};
// 			std::cout << "All packages sent." << std::endl;
		});


		testSuite.equal(clients[0]->template receive<int>(), 1, "CracenClient receive test");
		testSuite.equal(clients[0]->template receive<int>(), 2, "CracenClient receive test");
		testSuite.equal(clients[1]->template receive<int>(), 0, "CracenClient receive test");
		testSuite.equal(clients[1]->template receive<int>(), 2, "CracenClient receive test");
		testSuite.equal(clients[2]->template receive<int>(), 0, "CracenClient receive test");
		testSuite.equal(clients[2]->template receive<int>(), 1, "CracenClient receive test");

	}

	~CracenClientTest() {
		for(auto& clientPtr : clients) {
			clientPtr->stop();
		}
		server.stop();
	}

}; // End of class CracenServerTest


int main() {
	TestSuite testSuite("Cracen Server Test");

	//CracenClientTest<AsioStreamingSocket> tcpClientTest(testSuite);
	CracenClientTest<AsioDatagramSocket> udpClientTest(testSuite);

}
