#pragma once

#include "../BufferAdapter.hpp"
#include <string>

namespace cracen2 {

namespace network {

template <>
struct BufferAdapter<
	std::string
> :
	public ImmutableBuffer
{

	BufferAdapter(const std::string& input) :
		ImmutableBuffer(
			reinterpret_cast<decltype(ImmutableBuffer::data)>(&input[0]),
			input.size()*sizeof(char)
		)
	{};


	BufferAdapter(const ImmutableBuffer& other) :
		ImmutableBuffer(other)
	{};

	BufferAdapter(std::string&& other) = delete;

	std::string cast() const {
		std::string destination(size / sizeof(char), ' ');
		memcpy(
			&destination[0],
			data,
			size
		);
		return destination;
	}

}; // End of struct BufferAdapter

} // End of namespace network

} // End of namespace cracen2
