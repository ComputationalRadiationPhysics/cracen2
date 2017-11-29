#pragma once

#include <cinttypes>
#include <memory>

namespace cracen2 {

namespace network {

class Buffer {

	std::unique_ptr<std::uint8_t[]> buf;
	std::size_t count;

public:

	using value_type = std::uint8_t;

	Buffer(std::size_t size) :
		buf(new std::uint8_t[size]),
		count(size)
	{}

	Buffer() = default;
	Buffer(Buffer&&) = default;
	Buffer(const Buffer&) = delete;
	Buffer& operator=(Buffer&&) = default;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(std::unique_ptr<std::uint8_t[]>&& buf, const std::size_t size) :
		buf(std::move(buf)),
		count(size)
	{}

	void shrink(std::size_t size) {
		if(size > count) throw std::runtime_error("Can not shrink to a bigger size.");
		//buf.reset( new std::uint8_t[size] );
		count = size;
	}

	value_type* data() const {
		return buf.get();
	}

	std::size_t size() const {
		return count;
	}

	auto begin() {
		return data();
	}

	auto end() {
		return data() + size();
	}

};

struct ImmutableBuffer {

	const std::uint8_t* const data;
	const size_t size;

	ImmutableBuffer(const std::uint8_t* const data, size_t size);

}; // End of struct ImmutableBuffer

} // End of namespace network

} // End of namespace cracen2
