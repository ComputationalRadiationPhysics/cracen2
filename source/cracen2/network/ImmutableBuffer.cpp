#include "cracen2/network/ImmutableBuffer.hpp"

using namespace cracen2::network;

ImmutableBuffer::ImmutableBuffer(const std::uint8_t* const data, size_t size) :
	data(data),
	size(size)
{}
