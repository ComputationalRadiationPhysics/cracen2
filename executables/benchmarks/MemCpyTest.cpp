#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>

int main() {

	std::vector<std::size_t> sizes{
		1,
		8,
		32,
		256,
		1024,
		16*1024,
		64*1024,
		1024*1024,
		32*1024*1024,

	};

	for(auto size : sizes) {
		std::vector<std::uint8_t> base(size);
		std::vector<std::uint8_t> second(size);

		auto begin = std::chrono::high_resolution_clock::now();
		for(unsigned int i = 0; i < 1024; i++) {
			std::memcpy(second.data(), base.data(), size);
		}
		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time = end - begin;
		std::cout << "Framsize = " << size << " Bandwidht = " << (size * 1024 / time.count() / 1024 / 1024 / 1024 * 8) << " gbps." << std::endl;
	}

	return 0;
}
