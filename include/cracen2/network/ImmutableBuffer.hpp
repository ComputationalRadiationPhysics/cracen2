#pragma once

#include <cinttypes>
#include <vector>
#include <cstdlib>

namespace cracen2 {

namespace network {

class Buffer : public std::vector<std::uint8_t>{
private:
	using Base = std::vector<std::uint8_t>;
public:
	using Base::Base;
};

struct ImmutableBuffer {

	const std::uint8_t* const data;
	const size_t size;

	ImmutableBuffer(const std::uint8_t* const data, size_t size);

}; // End of struct ImmutableBuffer

} // End of namespace network

} // End of namespace cracen2
