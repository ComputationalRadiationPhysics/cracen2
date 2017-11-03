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
#include "cracen2/util/Unused.hpp"

namespace cracen2 {

namespace network {


/*
 * This class holds runtime type information and may be extendet to hold additional information.
 */
struct Header {
	std::uint16_t typeId;
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
	Header header;
	ImmutableBuffer body;

public:

	template <class ReturnType, class... Args>
	struct Visitor {

		using Result = ReturnType;

		Visitor() : argTuple(new std::tuple<Args...>()) {}
		Visitor(Visitor&&) = default;
		Visitor(const Visitor&) = default;

		Visitor& operator=(Visitor&&) = default;
		Visitor& operator=(const Visitor&) = default;

		std::array<
			std::function<ReturnType(const ImmutableBuffer&)>,
			std::tuple_size<TagList>::value
		> functions;

		std::shared_ptr<std::tuple<Args...>> argTuple;

		template <class Functor>
		int add(Functor&& functor);

	};

	template <class... Args>
	struct make_visitor_helper {

		template <class Functor, class... Rest>
		static auto make_visitor(Functor&& f1, Rest&&... functor);

	};


	Message(const ImmutableBuffer& buffer, const Header& header);

	template <class Type>
	Message(const Type& body);

	Message(Message&&) = default;
	Message& operator=(Message&&) = default;

	Message(const Message&) = default;
	Message& operator=(const Message&) = default;

	template <class Type>
	boost::optional<Type> cast();

	template <class ReturnType, class... Args>
	ReturnType visit(Visitor<ReturnType, Args...>& visitor);

	ImmutableBuffer& getBody();
	Header& getHeader();

}; // End of class Message

template <class TagList>
template <class ReturnType, class... Args>
template <class Functor>
int Message<TagList>::Visitor<ReturnType, Args...>::add(Functor&& functor) {

	// Extract type information from functor#
	using Result = typename cracen2::util::FunctionInfo<std::remove_reference_t<Functor>>::Result;
	using ArgumentList = typename cracen2::util::FunctionInfo<std::remove_reference_t<Functor>>::ParamList;
	static_assert(
		std::is_same<ReturnType, Result>::value,
		"Supplied functors must have the same return type."
	);
	/*
 	static_assert(
 		std::tuple_size<ArgumentList>::value == 1,
 			"Supplied functors for the visitor pattern must have a arity of one.\
 			The supplied functor has more or less parameters."
 	);
	*/

	// Since Functor hast exactly one argument, we can just take the first
	using Argument =
		typename std::tuple_element<
			0,
			ArgumentList
		>::type;

	TypeIdType id = cracen2::util::tuple_index<Argument, TagList>::value;
	functions[id] = [argTuple = this->argTuple, functor = std::forward<Functor>(functor)](const ImmutableBuffer& buffer) -> ReturnType {
		return functor(BufferAdapter<Argument>(buffer).cast(), std::get<Args>(*argTuple)...);
	};
	return 0;
}

template <class TagList>
template <class... Args>
template <class Functor, class... Rest>
auto Message<TagList>::make_visitor_helper<Args...>::make_visitor(Functor&& f1, Rest&&... functor) {

	Visitor<typename util::FunctionInfo<Functor>::Result, Args...> visitor;
	std::vector<int> temp { visitor.add(std::forward<Functor>(f1)), visitor.add(std::forward<Rest>(functor))... };

	return visitor;
}

template <class TagList>
Message<TagList>::Message(const ImmutableBuffer& body, const Header& header) :
	header(header),
	body(body)
{}

template <class TagList>
template <class Type>
Message<TagList>::Message(const Type& body) :
	header{ cracen2::util::tuple_index<Type, TagList>::value },
	body(make_buffer_adaptor(body))
{

	static_assert(
		cracen2::util::tuple_contains_type<Type, TagList>::value,
		"TagList must include the the type, that shall be casted into a message."
	);
}

template <class TagList>
template <class Type>
boost::optional<Type> Message<TagList>::cast() {
	if(header.typeId == cracen2::util::tuple_index<Type, TagList>::value) {
		const BufferAdapter<Type> adapter(
			ImmutableBuffer(
				body.data,
				body.size
			)
		);
		return adapter.cast();
	}
	return boost::none;
}

template <class TagList>
template <class ReturnType, class... Args>
ReturnType Message<TagList>::visit(Visitor<ReturnType, Args...>& visitor) {
	if(visitor.functions[header.typeId]) {
		return visitor.functions[header.typeId](
			ImmutableBuffer(
				body.data,
				body.size
			)
		);
	} else {
		const auto names = util::tuple_get_type_names<TagList>::value();
		std::string message("No visitor function for message of type \"");
		if(header.typeId < names.size()) message += util::demangle(names[header.typeId]) + "\" defined.";
		else  message += std::to_string(header.typeId) + "\" defined.";
		throw std::runtime_error(
			message
		);
		throw std::runtime_error(
			"No valid message received."
		);
	}
}

template <class TagList>
ImmutableBuffer& Message<TagList>::getBody() {
	return body;
}

template <class TagList>
Header& Message<TagList>::getHeader() {
	return header;
}

} // End of namespace network

} // End of namespace cracen2
