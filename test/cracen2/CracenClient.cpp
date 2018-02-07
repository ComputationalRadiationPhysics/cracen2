#include <memory>
#include <typeinfo>
#include <mutex>
#include <set>

#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/AsioDatagram.hpp"
#include "cracen2/sockets/AsioStreaming.hpp"

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

constexpr unsigned long Kilobyte = 1024;
constexpr unsigned long Megabyte = 1024*Kilobyte;
constexpr unsigned long Gigabyte = 1024*Megabyte;

constexpr size_t volume = 256*Megabyte;
//constexpr size_t volume = 2*Gigabyte;

const std::vector<size_t> frameSize {
//	1*Kilobyte,
	16*Kilobyte,
	64*Kilobyte,
	256*Kilobyte,
	512*Kilobyte,
	768*Kilobyte,
 	1*Megabyte,
	1500*Kilobyte,
	2*Megabyte,
	4*Megabyte,
	6*Megabyte,
	8*Megabyte,
	12*Megabyte,
	16*Megabyte,
	24*Megabyte,
	32*Megabyte,
	64*Megabyte,
	128*Megabyte,
	256*Megabyte
};
constexpr std::array<unsigned int, 3> participantsPerRole = {{2, 2, 2}};


template <class SocketImplementation>
struct CracenClientTest {

	using DataTagList = std::tuple<int>;
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
		)
	{
		constexpr std::array<
			std::pair<backend::RoleId, backend::RoleId>,
			3
		> roleGraph {{
			std::make_pair(0, 1),
			std::make_pair(1, 2),
			std::make_pair(2, 0)
		}};

		for(unsigned int role = 0; role < participantsPerRole.size(); role++) {
			for(unsigned int id = 0; id < participantsPerRole[role]; id++) {
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
				std::cout << "Push Cracen " << role*participantsPerRole.size() + id << std::endl;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		std::cout << "All Clients running." << std::endl;
// 		server.printStatus();
// 		for(auto& clientPtr : clients) {
// 			clientPtr->printStatus();
// 		}

		std::async(std::launch::async, [this](){
			for(auto& clientPtr : clients) {
				try {
					clientPtr->send(static_cast<int>(clientPtr->getRoleId()), send_policies::broadcast_any());
				} catch(const std::exception& e) {
					std::cerr << "Sending failed: " << e.what() << std::endl;
					testSuite.test(false, "Error during sending.");
				}
			};
			std::cout << "All packages sent." << std::endl;
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
			std::cout << "client shut down" << std::endl;
			//while(clientPtr->isRunning());
		}
 		server.stop();
	}

}; // End of class CracenServerTest


template <class SocketImplementation>
void benchmark() {
	using Chunk = std::vector<std::uint8_t>;
	using DataTagList = std::tuple<Chunk>;
	using CracenClient = cracen2::CracenClient<SocketImplementation, DataTagList>;
	using CracenServer = cracen2::CracenServer<SocketImplementation>;
	using RoleId = backend::RoleId;

	std::vector<std::pair<RoleId, RoleId>> roleGraph { std::make_pair(0 , 1) };

 	CracenServer server;

	CracenClient alice(server.getEndpoint(), 1, roleGraph);
	CracenClient bob(server.getEndpoint(), 0, roleGraph);

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

// 	alice.printStatus();
// 	bob.printStatus();

	JoiningThread bobThread(
		"CracenClientTest::bobThread",
		[&bob](){
			Chunk chunk;
			for(auto size : frameSize) {
				if(std::is_same<SocketImplementation, AsioDatagramSocket>::value && size > 64*Kilobyte) {
					break;
				}
				chunk.resize(size);

				std::vector<std::future<void>> requests;

				for(unsigned long sent = 0; sent < volume;sent+=size) {
					auto r = bob.asyncSend(chunk, send_policies::broadcast_role(1));
					for(auto& i : r) {
						requests.push_back(std::move(i));
					}
				}

				for(auto& r : requests) {
					r.get();
				}

			}
		});


		for(auto size : frameSize) {
			if(std::is_same<SocketImplementation, AsioDatagramSocket>::value && size > 64*Kilobyte) {
				break;
			}
			std::queue<std::future<Chunk>> requests;
			unsigned long received;
			auto begin = std::chrono::high_resolution_clock::now();
			{
				for(received = 0; received < volume;received+=size) {
					requests.push(alice.template asyncReceive<Chunk>());
				}
				unsigned int i = 0;
				while(requests.size() > 0) {
					try {
						requests.front().get();
						requests.pop();
					} catch(const std::exception& e) {
						std::cout << "alice catched exception on " << i << "th request: " << e.what() << std::endl;
					}
					i++;
				}
			}
			auto end = std::chrono::high_resolution_clock::now();

			std::cout
				<< "Bandwith of Communicator<" << demangle(typeid(SocketImplementation).name()) << ", ...> = "
				<< (static_cast<double>(received) * 1000 / Gigabyte / std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() * 8)
				<< " Gbps for " << size << " Byte Frames"
				<< std::endl;
		}

		bob.stop();
		alice.stop();

		server.stop();
}

int main() {
	TestSuite testSuite("Cracen Client Test");

	{ CracenClientTest<BoostMpiSocket> mpiClientTest(testSuite); }
	{ CracenClientTest<AsioDatagramSocket> udpClientTest(testSuite); }
	{ CracenClientTest<AsioStreamingSocket> tcpClientTest(testSuite); }
//
	benchmark<BoostMpiSocket>();
	benchmark<AsioStreamingSocket>();

	benchmark<BoostMpiSocket>();

	return 0;
}
