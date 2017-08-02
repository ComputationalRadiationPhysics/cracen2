#pragma once

#include <cinttypes>
#include <vector>
#include <cstdlib>

namespace cracen2 {

namespace network {

using Buffer = std::vector<std::uint8_t>;

struct ImmutableBuffer {

	const std::uint8_t* const data;
	const size_t size;

	ImmutableBuffer(const std::uint8_t* const data, size_t size);

}; // End of struct ImmutableBuffer

} // End of namespace network

} // End of namespace cracen2
