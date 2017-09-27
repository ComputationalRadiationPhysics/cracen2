#include <memory>
#include <typeinfo>
#include <mutex>
#include <set>

#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/Asio.hpp"

std::ostream& operator<<(std::ostream& lhs, const cracen2::sockets::BoostMpiSocket::Endpoint& rhs) {
	lhs << "{ " << rhs.first << ", " << rhs.second << " }";
	return lhs;
}

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/CracenClient.hpp"
#include "cracen2/send_policies/broadcast.hpp"
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

constexpr std::array<unsigned int, 3> participantsPerRole = {{2, 2, 2}};


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

	CracenClientTest(TestSuite& parent) :
		testSuite(
			std::string("Implementation test for ")+demangle(typeid(SocketImplementation).name()),
			std::cout,
			&parent
		),
		server(
			Endpoint()
		)
	{
		std::cout << std::endl;
		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int id = 0; id < participantsPerRole[role]; id++) {
				std::cout << "Push Cracen " << role*participantsPerRole.size() + id << std::endl;
				clients.push_back(
					std::unique_ptr<CracenClient>(
						new CracenClient
						(
							server.getEndpoint(),
							role,
							roleGraph
						)
					)
				);
				clients.back()->printStatus();
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		std::cout << "All Clients running." << std::endl;
		server.printStatus();
		for(auto& clientPtr : clients) {
			clientPtr->printStatus();
		}

		std::async(std::launch::async, [this](){
			for(auto& clientPtr : clients) {
				try {
					clientPtr->send(static_cast<int>(clientPtr->getRoleId()), send_policies::broadcast_any());
				} catch(const std::exception& e) {
					std::cerr << "Sending failed: " << e.what() << std::endl;
					testSuite.test(false, "Error during sending.");
				}
			};
// 			std::cout << "All packages sent." << std::endl;
		});

		for(auto& client :  clients) {
			for(unsigned int j = 0; j < 2; j++) {
			auto i = client->template receive<int>();
			const auto roleId = client->getRoleId();
				testSuite.test(
					i != static_cast<int>(roleId),
					std::string("CracenClient(") + std::to_string(roleId) + ") receive test: i = " + std::to_string(i) + " should not be " + std::to_string(roleId)
				);
			}
		}

		std::cout << "All tests run." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	~CracenClientTest() {
		std::cout << "Finished." << std::endl;
		for(auto& clientPtr : clients) {
			clientPtr->stop();
			while(clientPtr->isRunning());
		}
 		server.stop();
	}

}; // End of class CracenServerTest

int main() {
	TestSuite testSuite("Cracen Server Test");

	{ CracenClientTest<AsioStreamingSocket> tcpClientTest(testSuite); }
	{ CracenClientTest<AsioDatagramSocket> udpClientTest(testSuite); }
	{ CracenClientTest<BoostMpiSocket> mpiClientTest(testSuite); }

	return 0;
}
