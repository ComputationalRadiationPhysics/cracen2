#pragma once

#include <future>

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

	template <class ReturnType = void>
	using Visitor = typename Message::template Visitor<ReturnType>;

	template <class... Functors>
	static decltype(Message::make_visitor(std::declval<Functors>()...)) make_visitor(Functors&&... functors);

	using typename Socket::Endpoint;
	using Socket::bind;
	using Socket::isOpen;
	using Socket::getLocalEndpoint;
	using Socket::close;

	Communicator() : Socket() {};
	~Communicator() = default;

	Communicator(Communicator&& other) = default; //: Socket(std::forward<Socket>(other)) {};
	Communicator& operator=(Communicator&& other) = default;

	Communicator(Socket&& other) : Socket(std::forward<Socket>(other)) {};

	Communicator(const Communicator& other) = delete;
	Communicator& operator=(const Communicator& other) = delete;

	template <class T>
	void sendTo(const T& data, const Endpoint remote);

	template <class T>
	std::future<void> asyncSendTo(const T& data, const Endpoint remote);

	// This has to be used with extreme caution. Guessing the wrong type will cause packages to be droped and exception to be thrown
	template <class T>
	std::pair<T, Endpoint> receiveFrom();

	// This has to be used with extreme caution. Guessing the wrong type will cause packages to be droped and exception to be thrown
	template <class T>
	T receive();

	template<class T>
	std::future<std::pair<T, Endpoint>> asyncReceiveFrom();

	template<class T>
	std::future<T> asyncReceive();

	template <class Visitor>
	typename std::remove_reference_t<Visitor>::Result receive(Visitor&& visitor);

	template <class Visitor>
	std::future<typename std::remove_reference_t<Visitor>::Result> asyncReceive(Visitor&& visitor);

}; // End of class Communicator

template <class Socket, class TagList>
template <class... Functors>
decltype(Communicator<Socket, TagList>::Message::make_visitor(std::declval<Functors>()...)) Communicator<Socket, TagList>::make_visitor(Functors&&... functors) {
	return Message::make_visitor(std::forward<Functors>(functors)...);
};

template <class Socket, class TagList>
template <class T>
void Communicator<Socket, TagList>::sendTo(const T& data, const Endpoint remote) {
	return asyncSendTo(data, remote).get();
}

template <class Socket, class TagList>
template <class T>
std::future<void> Communicator<Socket, TagList>::asyncSendTo(const T& data, const Endpoint remote) {
	Message message(data);
	return Socket::asyncSendTo(ImmutableBuffer(message.getBuffer().data(), message.getBuffer().size()), remote);
}

template <class Socket, class TagList>
template <class T>
std::pair<T, typename Communicator<Socket, TagList>::Endpoint> Communicator<Socket, TagList>::receiveFrom() {
	return asyncReceiveFrom<T>().get();
}

template <class Socket, class TagList>
template <class T>
T Communicator<Socket, TagList>::receive() {
	return asyncReceive<T>().get();
}

template <class Socket, class TagList>
template <class T>
std::future<std::pair<T, typename Communicator<Socket, TagList>::Endpoint>> Communicator<Socket, TagList>::asyncReceiveFrom() {
	auto datagramFuture = Socket::asyncReceiveFrom();
	return std::async(
		[datagramFuture = std::move(datagramFuture)]() mutable -> std::pair<T, Endpoint> {
			auto datagram = datagramFuture.get();
			Message message(std::move(datagram.first));
			boost::optional<T> result = message.template cast<T>();
			if(result) {
				return std::make_pair(std::move(result.get()), datagram.second);
			} else {
				auto typeId = message.getTypeId();

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
	);
}

template <class Socket, class TagList>
template <class T>
std::future<T> Communicator<Socket, TagList>::asyncReceive() {
	auto f = asyncReceiveFrom<T>();
	return std::async(
		[f = std::move(f)]() mutable -> T {
			return f.get().first;
		}
	);
}

template <class Socket, class TagList>
template <class Vis>
typename std::remove_reference_t<Vis>::Result Communicator<Socket, TagList>::receive(Vis&& visitor) {
	return asyncReceive(std::forward<Vis>(visitor)).get();
}

template <class Socket, class TagList>
template <class Vis>
std::future<typename std::remove_reference_t<Vis>::Result> Communicator<Socket, TagList>::asyncReceive(Vis&& visitor) {
	auto datagram = Socket::asyncReceiveFrom();

	return std::async(
		[datagramFuture = std::move(datagram), visitor = std::forward<Vis>(visitor)]() mutable
			-> typename std::remove_reference_t<Vis>::Result
		{
			Message message(std::move(datagramFuture.get().first));
			return message.visit(visitor);
		}
	);

}

} // End of namespace network

} // End of namespace cracen2
