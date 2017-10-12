#include <vector>
#include <cstdint>
#include <chrono>
#include <iostream>

#include "cracen2/sockets/BoostMpi.hpp"

using namespace cracen2::sockets;
constexpr unsigned int runs = 1024;

int main() {

	BoostMpiSocket server;
	server.bind();

	const auto serverEp = server.getLocalEndpoint();

	BoostMpiSocket client;
	client.bind();

	std::vector<std::size_t> sizes{
// 		1,
// 		8,
// 		32,
// 		256,
// 		1024,
		16*1024,
		64*1024,
		1024*1024,
// 		32*1024*1024,
	};

	for(auto size : sizes) {
		std::vector<std::uint8_t> base(size);
		std::vector<std::uint8_t> second(size);
		std::vector<std::future<void>> sendRequests(runs);
		std::vector<std::future<BoostMpiSocket::Datagram>> receiveRequests(runs);

		auto begin = std::chrono::high_resolution_clock::now();

		for(unsigned int i = 0; i < runs; i++) {
			sendRequests[i] = client.asyncSendTo(cracen2::network::ImmutableBuffer(base.data(), base.size()) , serverEp);
			receiveRequests[i] = server.asyncReceiveFrom();
		}

		for(unsigned int i = 0; i < runs; i++) {
			receiveRequests[i].get();
			sendRequests[i].get();
		}

		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time = end - begin;
		std::cout << "Framsize = " << size << " Bandwidht = " << (size * 1024 / time.count() / 1024 / 1024 / 1024 * 8) << " gbps." << std::endl;
	}

	return 0;
}
