#pragma once

#include "Message.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Tuple.hpp"

namespace cracen2 {

namespace network {

/*
 * The Communicator combines the abstraction of the network::Socket and network::Message
 */
template<class Socket, class TagList>
class Communicator : private Socket

{
public:

	using Message = cracen2::network::Message<TagList>;
	using Visitor = typename Message::Visitor;

	using typename Socket::Endpoint;
	using Socket::connect;
	using Socket::accept;
	using Socket::bind;
	using Socket::isOpen;
	using Socket::getLocalEndpoint;
	using Socket::getRemoteEndpoint;
	using Socket::close;

	Communicator() : Socket() {};
	~Communicator() = default;

	Communicator(Communicator&& other) = default; //: Socket(std::forward<Socket>(other)) {};
	Communicator& operator=(Communicator&& other) = default;

	Communicator(Socket&& other) : Socket(std::forward<Socket>(other)) {};

	Communicator(const Communicator& other) = delete;
	Communicator& operator=(const Communicator& other) = delete;

	template <class T>
	void send(const T& data);

	// This has to be used with extreme caution. Guessing the wrong type will cause packages to be droped and exception to be thrown
	template <class T>
	T receive();

	void receive(Visitor visitor);

}; // End of class Communicator

template <class Socket, class TagList>
template <class T>
void Communicator<Socket, TagList>::send(const T& data) {
	Message message(data);
	Socket::send(ImmutableBuffer(message.getBuffer().data(), message.getBuffer().size()));
}

template <class Socket, class TagList>
template <class T>
T Communicator<Socket, TagList>::receive() {
	Message message(Socket::receive());

	boost::optional<T> result = message.template cast<T>();
	if(result) {
		return std::move(result.get());
	} else {
		decltype(message.getTypeId()) typeId;
		try {
			typeId = message.getTypeId();
		} catch(const std::exception& e) {
			std::cerr << "Exception thrown while getting type id of message." << std::endl;
			std::cerr << e.what() << std::endl;
		}
		const auto typeNames = util::tuple_get_type_names<TagList>::value();

		std::string error("Trying to receive a message with a wrong type. MessageTypeId = " + std::to_string(typeId) + "\n");
		if(typeId < typeNames.size()) {
			error +=
				util::demangle(typeNames[typeId]) +
				" != " +
				util::demangle(typeid(T).name());
		} else {
			error += "TypeIndex > TagList.size() (" + std::to_string(std::tuple_size<TagList>::value) + ")";
		}
		throw(
			std::runtime_error(
				error
			)
		);
	}
}

template <class Socket, class TagList>
void Communicator<Socket, TagList>::receive(Visitor visitor) {
	Message message(Socket::receive());

	message.visit(visitor);
}

} // End of namespace network

} // End of namespace cracen2
