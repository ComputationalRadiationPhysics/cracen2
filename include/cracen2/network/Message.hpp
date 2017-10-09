#pragma once

#include <cstring>
#include <cstdint>
#include <tuple>
#include <type_traits>
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

	template <class ReturnType>
	struct Visitor {

		using Result = ReturnType;

		Visitor() = default;
		Visitor(Visitor&&) = default;
		Visitor(const Visitor&) = default;

		Visitor& operator=(Visitor&&) = default;
		Visitor& operator=(const Visitor&) = default;

		std::array<
			std::function<ReturnType(const ImmutableBuffer&)>,
			std::tuple_size<TagList>::value
		> functions;

		template <class Functor>
		int add(Functor&& functor);

	};

	template <class Functor, class... Rest>
	static auto make_visitor(Functor&& f1, Rest&&... functor);

	Message(Buffer&& buffer);

	template <class Type>
	Message(const Type& body);

	Message(Message&&) = default;
	Message& operator=(Message&&) = default;

	template <class Type>
	boost::optional<Type> cast();

	template <class ReturnType>
	ReturnType visit(Visitor<ReturnType>& visitor);

	Buffer& getBuffer();

	TypeIdType getTypeId();

}; // End of class Message

template <class TagList>
template <class ReturnType>
template <class Functor>
int Message<TagList>::Visitor<ReturnType>::add(Functor&& functor) {

	// Extract type information from functor#
	using Result = typename cracen2::util::FunctionInfo<std::remove_reference_t<Functor>>::Result;
	using ArgumentList = typename cracen2::util::FunctionInfo<std::remove_reference_t<Functor>>::ParamList;
	static_assert(
		std::is_same<ReturnType, Result>::value,
		"Supplied functors must have the same return type."
	);
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
	functions[id] = [functor = std::forward<Functor>(functor)](const ImmutableBuffer& buffer) -> ReturnType {
		return functor(BufferAdapter<Argument>(buffer).cast());
	};
	return 0;
}

template <class TagList>
template <class Functor, class... Rest>
auto Message<TagList>::make_visitor(Functor&& f1, Rest&&... functor) {

	Visitor<typename util::FunctionInfo<Functor>::Result> visitor;
	std::vector<int> temp { visitor.add(std::forward<Functor>(f1)), visitor.add(std::forward<Rest>(functor))... };

	return visitor;
}

template <class TagList>
Message<TagList>::Message(Buffer&& buffer) :
	buffer(std::move(buffer))
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

	buffer = Buffer(headerBuffer.size + bodyBuffer.size);
	std::memcpy(buffer.data(), headerBuffer.data, headerBuffer.size);
	std::memcpy(buffer.data() + headerBuffer.size, bodyBuffer.data, bodyBuffer.size);
}

template <class TagList>
template <class Type>
boost::optional<Type> Message<TagList>::cast() {
	if(buffer.size() > sizeof(Header)) {
		Header* header = reinterpret_cast<Header*>(buffer.data());
 		if(header->typeId == cracen2::util::tuple_index<Type, TagList>::value) {
			const BufferAdapter<Type> adapter(
				ImmutableBuffer(
					buffer.data() + sizeof(Header),
					buffer.size() - sizeof(Header)
				)
			);
			return adapter.cast();
		}
	}
	return boost::none;
}

template <class TagList>
template <class ReturnType>
ReturnType Message<TagList>::visit(Visitor<ReturnType>& visitor) {
	if(buffer.size() >= sizeof(Header)) {
		Header* header = reinterpret_cast<Header*>(buffer.data());
		const TypeIdType typeId = header->typeId;
		if(visitor.functions[typeId]) {
			return visitor.functions[typeId](
				ImmutableBuffer(
					buffer.data() + sizeof(Header),
					buffer.size() - sizeof(Header)
				)
			);
		}
		const auto names = util::tuple_get_type_names<TagList>::value();
		std::string message("No visitor function for message of type \"");
		if(typeId < names.size()) message += util::demangle(names[typeId]) + "\" defined.";
		else  message += std::to_string(typeId) + "\" defined.";
		throw std::runtime_error(
			message
		);
	}
	throw std::runtime_error(
		"No valid message received."
	);
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
