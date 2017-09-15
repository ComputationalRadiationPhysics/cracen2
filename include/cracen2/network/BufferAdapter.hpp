#pragma once

#include <type_traits>
#include <vector>
#include <array>
#include <cstddef> // size_t
#include <cstring> // memcpy
#include <iostream>

#include "ImmutableBuffer.hpp"

namespace cracen2 {

namespace network {

template <class Type>
using  linear_memory_check = std::is_trivially_destructible<Type>;
//using  linear_memory_check = std::is_pod<Type>;
//using  linear_memory_check = std::is_trivial<Type>;
//using  linear_memory_check = std::is_trivially_copyable<Type>;
//using  linear_memory_check = std::is_standard_layout<Type>;

template <class Type, class enable = void>
struct BufferAdapter; // End of class BufferAdapter


template <
	class Type
>
struct BufferAdapter<
	Type,
	typename std::enable_if<
		linear_memory_check<Type>::value
	>::type
> :
	public ImmutableBuffer
{
	BufferAdapter(const Type& input) :
		ImmutableBuffer(
			reinterpret_cast<decltype(ImmutableBuffer::data)>(&input),
			sizeof(Type)
		)
	{};

	BufferAdapter(const ImmutableBuffer& other) :
		ImmutableBuffer(other)
	{};

	BufferAdapter(Type&& other) = delete;

	Type cast() const {
		Type result;
		memcpy(
			&result,
			data,
			std::min(size, sizeof(result))
		);
		return result;
	}

}; // End of struct BufferAdapter

template <class Type>
BufferAdapter<Type> make_buffer_adaptor(const Type& input) {
	return BufferAdapter<Type> { input };
}

template <class Type>
BufferAdapter<Type> make_buffer_adaptor(Type& input) {
	return BufferAdapter<Type> { input };
}

template <class Type>
BufferAdapter<Type> make_buffer_adaptor(Type&& input) = delete;

} // End of namespace network

} // End of namespace cracen2
