#pragma once

#include <cstring>
#include <cstdint>
#include <tuple>
#include <boost/optional.hpp>

#include "cracen2/network/adapter/All.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Tuple.hpp"
#include "cracen2/util/Function.hpp"

namespace cracen2 {

namespace network {


/*
 * This class holds runtime type information and may be extendet to hold additional information.
 */
struct Header {
	std::uint16_t typeId;

	Header(std::uint16_t typeId);
	~Header() = default;
};

/*
 * This is the message class. It builds ontop of the BufferAdapter and provides a interface to go
 * from a typed value, to a untyped buffer with runtime type information in it and back again.
 */
template <class TagList> // TagList = std::tuple<...> of all possible message types
class Message {

	static_assert(cracen2::util::is_std_tuple<TagList>::value, "The TagList, must be a std::tuple of all, that can be transformed into a message.");
	static_assert(cracen2::util::tuple_duplicate_free<TagList>::value, "The TagList must be free of duplicate types.");

private:

	using TypeIdType = decltype(Header::typeId);
	Buffer buffer;

public:

	struct Visitor {

		std::array<
			std::function<void(const ImmutableBuffer&)>,
			std::tuple_size<TagList>::value >
		functions;

		template <class Functor>
		int add(const Functor& functor);

		template <class... Functor>
		Visitor(Functor... functor);

	};

	Message(Buffer&& buffer);

	template <class Type>
	Message(const Type& body);

	template <class Type>
	boost::optional<Type> cast();

	void visit(Visitor visitor);

	Buffer& getBuffer();

	TypeIdType getTypeId();

}; // End of class Message

template <class TagList>
template <class... Functor>
Message<TagList>::Visitor::Visitor(Functor... functor) {
	std::vector<int> temp { add(functor)... };
}

template <class TagList>
template <class Functor>
int Message<TagList>::Visitor::add(const Functor& functor) {

	// Extract type information from functor
	using ArgumentList = typename cracen2::util::FunctionInfo<Functor>::ParamList;
	static_assert(
		std::tuple_size<ArgumentList>::value == 1,
			"Supplied functors for the visitor pattern must have a arity of one.\
			The supplied functor has more or less parameters."
	);
	// Since Functor hast exactly one argument, we can just take the first
	using Argument =
		typename std::tuple_element<
			0,
			ArgumentList
		>::type;


	TypeIdType id = cracen2::util::tuple_index<Argument, TagList>::value;
	functions[id] = [functor](const ImmutableBuffer& buffer){
		Argument argument;
		BufferAdapter<Argument>(buffer).copyTo(argument);
		functor(argument);
	};
	return 0;
}

template <class TagList>
Message<TagList>::Message(Buffer&& buffer) :
	buffer(buffer)
{}

template <class TagList>
template <class Type>
Message<TagList>::Message(const Type& body) {
	static_assert(
		cracen2::util::tuple_contains_type<Type, TagList>::value,
		"TagList must include the the type, that shall be casted into a message."
	);

	const Header header(cracen2::util::tuple_index<Type, TagList>::value);
	const ImmutableBuffer bodyBuffer = make_buffer_adaptor(body);
	const ImmutableBuffer headerBuffer = make_buffer_adaptor(header);

	buffer.resize(headerBuffer.size + bodyBuffer.size);
	std::memcpy(buffer.data(), headerBuffer.data, headerBuffer.size);
	std::memcpy(buffer.data() + headerBuffer.size, bodyBuffer.data, bodyBuffer.size);
}

template <class TagList>
template <class Type>
boost::optional<Type> Message<TagList>::cast() {
	if(buffer.size() > sizeof(Header)) {
		Header* header = reinterpret_cast<Header*>(buffer.data());
 		if(header->typeId == cracen2::util::tuple_index<Type, TagList>::value) {
			Type result;
			const BufferAdapter<Type> adapter(
				ImmutableBuffer(
					buffer.data() + sizeof(Header),
					buffer.size() - sizeof(Header)
				)
			);
			adapter.copyTo(result);
			return std::move(result);
		}
	}
	return boost::none;
}

template <class TagList>
void Message<TagList>::visit(Visitor visitor) {
	if(buffer.size() > sizeof(Header)) {
		Header* header = reinterpret_cast<Header*>(buffer.data());
		const TypeIdType typeId = header->typeId;
		if(visitor.functions[typeId]) {
			visitor.functions[typeId](
				ImmutableBuffer(
					buffer.data() + sizeof(Header),
					buffer.size() - sizeof(Header)
				)
			);
		} else {
			throw std::runtime_error(
				std::string("No visitor function for message of type \"") +
				util::demangle(util::tuple_get_type_names<TagList>::value()[typeId])
				+ "\" defined."
			);
		}
	}
}

template <class TagList>
Buffer& Message<TagList>::getBuffer() {
	return buffer;
}

template <class TagList>
typename Message<TagList>::TypeIdType Message<TagList>::getTypeId() {
	if(buffer.size() > sizeof(Header)) {
		Header* header = reinterpret_cast<Header*>(buffer.data());
		return header->typeId;
	} else {
		throw std::runtime_error("Try to get type id of empty message.");
	}

}

} // End of namespace network

} // End of namespace cracen2
