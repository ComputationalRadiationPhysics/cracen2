#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/AsioDatagram.hpp"
#include "cracen2/util/Demangle.hpp"

#include <chrono>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;


std::promise<AsioDatagramSocket::Endpoint> udpEndpoint;
//std::promise<AsioStreamingSocket::Endpoint> tcpEndpoint;

constexpr auto runs = 30;

constexpr unsigned long Kilobyte = 1024;
constexpr unsigned long Megabyte = 1024*Kilobyte;
constexpr unsigned long Gigabyte = 1024*Megabyte;

constexpr size_t volume = 256*Megabyte;
// constexpr size_t volume = 5*Gigabyte;

const std::vector<size_t> frameSize {
// 	1*Kilobyte,
	16*Kilobyte,
	64*Kilobyte - 128,
	256*Kilobyte,
	512*Kilobyte,
 	1*Megabyte,
	2*Megabyte
};

template <class Socket>
struct SocketTest {
	using Endpoint = typename Socket::Endpoint;

	static constexpr const char* message = "Hello World!";
	SocketTest(TestSuite& testSuite) {

		Socket sink;
		sink.bind();

		const Endpoint sinkEndpoint = sink.getLocalEndpoint();
		std::cout << "sinkEp = " << sinkEndpoint << std::endl;

		JoiningThread source([sinkEndpoint, &testSuite](){
			Socket source;
			source.bind();
			std::cout << "sourceEp = " << source.getLocalEndpoint() << std::endl;
			std::string s(message);
			ImmutableBuffer buffer(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
			for(int i = 0; i < runs; i++) {
// 				std::cout << "source send " << i << " / " << runs << std::endl;
				auto res = source.asyncSendTo(buffer, sinkEndpoint);
				res.get();
			}

			for(int i = 0; i < runs; i++) {
			// Sink part
				const auto buffer = source.asyncReceiveFrom().get().body;
				std::string s(reinterpret_cast<const char*>(buffer.data()), buffer.size());
				testSuite.equal(s, std::string(message), "Send/Receive test for " + getTypeName<Socket>());
			}
		});


		Endpoint sourceEp;
		for(int i = 0; i < runs; i++) {
		// Sink part
			try {
				auto datagram = sink.asyncReceiveFrom().get();
				const auto buffer = std::move(datagram.body);
				if(i == 0) {
					sourceEp = datagram.remote;
					std::cout << "resolvedSourceEp = " << sourceEp << std::endl;
				}
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
			sink.asyncSendTo(buffer, sourceEp).get();
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
				const auto buffer = sink.asyncReceiveFrom().get().body;
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


template <class Socket>
void benchmark() {
	Socket source;
	source.bind();
	Socket sink;
	sink.bind();

	const auto sinkEp = sink.getLocalEndpoint();

	std::vector<std::future<void>> send_requests;
	std::vector<std::future<typename Socket::Datagram>> receive_requests;

	for(auto size : frameSize) {

		Buffer chunk(size);

		send_requests.reserve(volume/size + 1);
		receive_requests.reserve(volume/size + 1);

		auto begin = std::chrono::high_resolution_clock::now();
		size_t i;
		for(i = 0; i*size <= volume; i++) {
			receive_requests.push_back(sink.asyncReceiveFrom());
			send_requests.push_back(source.asyncSendTo(ImmutableBuffer(chunk.data(), chunk.size()), sinkEp));
		}
		for(i = 0; i*size <= volume; i++) {
			send_requests[i].wait();
			receive_requests[i].wait();
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> time = end - begin;
		std::cout << "Badwidth for " << size << " Byte frames = " << i*size / time.count() / Gigabyte * 8 << " gbps." << std::endl;

		send_requests.clear();
		receive_requests.clear();
	}
}


int main() {

	TestSuite testSuite("Asio");

	std::cout << "Single Test" << std::endl;
//  { SocketTest<AsioStreamingSocket> test(testSuite); }
//  { SocketTest<AsioDatagramSocket> test(testSuite); }
	{ SocketTest<BoostMpiSocket> test(testSuite); }

	std::cout << "Multi Test" << std::endl;
//  { MultiSocketTest<AsioStreamingSocket> test(testSuite); }
//  { MultiSocketTest<AsioDatagramSocket> test(testSuite); }
	{ MultiSocketTest<BoostMpiSocket> test(testSuite); }

	std::cout << "Benchmark" << std::endl;
	benchmark<BoostMpiSocket>();

}
