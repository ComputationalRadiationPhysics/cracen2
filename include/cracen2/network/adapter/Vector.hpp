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
		return std::vector<Type>(reinterpret_cast<const Type*>(data), reinterpret_cast<const Type*>(data+size));;
	}

}; // End of struct BufferAdapter

} // End of namespace network

} // End of namespace cracen2
