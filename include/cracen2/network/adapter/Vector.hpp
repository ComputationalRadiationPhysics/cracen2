#pragma once

#include "../BufferAdapter.hpp"
#include <vector>
#include <cstdint>

namespace cracen2 {

namespace network {

template <
	class Type
>
struct BufferAdapter<
	std::vector<Type>,
	typename std::enable_if<
		linear_memory_check<Type>::value
	>::type
>
	: public ImmutableBuffer
{

	BufferAdapter(const std::vector<Type>& input) :
		ImmutableBuffer(reinterpret_cast<decltype(ImmutableBuffer::data)>(input.data()), input.size()*sizeof(Type))
	{};

	BufferAdapter(const ImmutableBuffer& other) :
		ImmutableBuffer(other)
	{};

	BufferAdapter(std::vector<Type>&& other) = delete;

	std::vector<Type> cast() const {
		std::vector<Type> destination(size / sizeof(Type));
		memcpy(
			destination.data(),
			data,
			size
		);
		return destination;
	}

}; // End of struct BufferAdapter

} // End of namespace network

} // End of namespace cracen2
