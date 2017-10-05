#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/AsioDatagram.hpp"
#include "cracen2/util/Demangle.hpp"
#include <future>


using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;


std::promise<AsioDatagramSocket::Endpoint> udpEndpoint;
//std::promise<AsioStreamingSocket::Endpoint> tcpEndpoint;

constexpr auto runs = 30;

template <class Socket>
struct SocketTest {
	using Endpoint = typename Socket::Endpoint;

	static constexpr const char* message = "Hello World!";
	SocketTest(TestSuite& testSuite) {

		Socket sink;
		try {
			sink.bind();
		} catch(const std::exception& e) {
			std::cout << e.what() << std::endl;
		}

		const Endpoint sinkEndpoint = sink.getLocalEndpoint();
		JoiningThread source([sinkEndpoint, &testSuite](){
			Socket source;

			std::string s(message);
			ImmutableBuffer buffer(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
			for(int i = 0; i < runs; i++) {
// 				std::cout << "source send " << i << " / " << runs << std::endl;
				auto res = source.asyncSendTo(buffer, sinkEndpoint);
				res.get();
			}

			for(int i = 0; i < runs; i++) {
			// Sink part
				const auto buffer = source.asyncReceiveFrom().get().first;
				std::string s(reinterpret_cast<const char*>(buffer.data()), buffer.size());
				testSuite.equal(s, std::string(message), "Send/Receive test for " + getTypeName<Socket>());
			}
		});

		for(int i = 0; i < runs; i++) {
		// Sink part
			try {
				const auto buffer = sink.asyncReceiveFrom().get().first;
				std::string s(reinterpret_cast<const char*>(buffer.data()), buffer.size());
				testSuite.equal(s, std::string(message), "Send/Receive test for " + getTypeName<Socket>());
// 				std::cout << "received " << i << " / " << runs << std::endl;
			} catch(std::exception& e) {
// 				std::cerr << "receive threw error:" << e.what() << std::endl;
				i--;
				continue;
			}
		}

		std::string s(message);
		ImmutableBuffer buffer(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
		for(int i = 0; i < runs; i++) {
			sink.asyncSendTo(buffer, sinkEndpoint).get();
		}
	}
};


template <class Socket>
struct MultiSocketTest {
	using Endpoint = typename Socket::Endpoint;

	static constexpr const char* message = "Hello World!";
	MultiSocketTest(TestSuite& testSuite) {

		Socket sink;
		try {
			sink.bind();
		} catch(const std::exception& e) {
			std::cout << e.what() << std::endl;
		}

		const Endpoint sinkEndpoint = sink.getLocalEndpoint();
		std::vector<JoiningThread> sourceThreads;
		for(int i = 0; i < 3; i++) {
			sourceThreads.emplace_back([sinkEndpoint](){
				Socket source;

				for(int i = 0; i < runs; i++) {
					ImmutableBuffer buffer(reinterpret_cast<const std::uint8_t*>(&i), sizeof(i));
					source.asyncSendTo(buffer, sinkEndpoint).get();
				}
			});
// 			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}


		for(int i = 0; i < 3*runs; i++) {
		// Sink part
			try {
				const auto buffer = sink.asyncReceiveFrom().get().first;
				const int j = *reinterpret_cast<const int*>(buffer.data());
// 				std::cout << j << std::endl;
				testSuite.test(j >= 0 && j < runs, "Send/Receive test for " + getTypeName<Socket>());
			} catch(std::exception& e) {
// 				std::cerr << "receive threw error:" << e.what() << std::endl;
				i--;
				continue;
			}
		}
		std::cout << "sink finished." << std::endl;
	}
};

int main() {

	TestSuite testSuite("Asio");

	std::cout << "Single Test" << std::endl;
//  	{ SocketTest<AsioStreamingSocket> test(testSuite); }
//  	{ SocketTest<AsioDatagramSocket> test(testSuite); }
	{ SocketTest<BoostMpiSocket> test(testSuite); }

	std::cout << "Multi Test" << std::endl;
//  	{ MultiSocketTest<AsioStreamingSocket> test(testSuite); }
//  	ś{ MultiSocketTest<AsioDatagramSocket> test(testSuite); }
	{ MultiSocketTest<BoostMpiSocket> test(testSuite); }

}
