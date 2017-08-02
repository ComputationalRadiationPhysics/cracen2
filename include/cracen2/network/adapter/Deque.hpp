#pragma once

#include "../BufferAdapter.hpp"
#include <deque>

namespace cracen2 {

namespace network {

template <
	class Type
>
struct BufferAdapter<
	std::deque<Type>,
	typename std::enable_if<
		std::is_trivially_copyable<Type>::value
	>::type
> :
	public ImmutableBuffer
{
	using ImmutableBuffer::ImmutableBuffer;

	BufferAdapter(const std::deque<Type>& input) :
		ImmutableBuffer(
			reinterpret_cast<decltype(ImmutableBuffer::data)>(&input[0]),
			input.size()*sizeof(Type)
		)
	{};

	BufferAdapter(const ImmutableBuffer& other) :
		ImmutableBuffer(other)
	{};

	BufferAdapter(std::deque<Type>&& other) = delete;

	void copyTo(std::deque<Type>& destination) const {
		destination.resize(size / sizeof(Type));
		memcpy(
			&destination[0],
			data,
			size
		);
	}

}; // End of struct BufferAdapter

} // End of namespace network

} // End of namespace cracen2
