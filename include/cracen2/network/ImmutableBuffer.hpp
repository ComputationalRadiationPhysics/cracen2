#pragma once

#include <cinttypes>
#include <vector>
#include <cstdlib>

namespace cracen2 {

namespace network {

class Buffer : public std::vector<std::uint8_t>{
public:
	using Base = std::vector<std::uint8_t>;
	using Base::Base;
	Buffer(Base&& base) :
		Base(base)
	{}
	Buffer(const Base& base) :
		Base(base)
	{}
};

struct ImmutableBuffer {

	const std::uint8_t* const data;
	const size_t size;

	ImmutableBuffer(const std::uint8_t* const data, size_t size);

}; // End of struct ImmutableBuffer

} // End of namespace network

} // End of namespace cracen2
