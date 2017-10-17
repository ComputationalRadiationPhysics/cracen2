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

	using typename Socket::Endpoint;

	template <class... Functors>
	static auto make_visitor(Functors&&... functors);

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
auto Communicator<Socket, TagList>::make_visitor(Functors&&... functors) {
	return Message::template make_visitor_helper<Endpoint>::make_visitor(std::forward<Functors>(functors)...);
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
	auto& header = message.getHeader();
	return Socket::asyncSendTo(message.getBody(), remote, ImmutableBuffer(reinterpret_cast<std::uint8_t*>(&header), sizeof(header)));
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
		std::launch::deferred,
		[datagramFuture = std::move(datagramFuture)]() mutable -> std::pair<T, Endpoint> {
			auto datagram = datagramFuture.get();
			Message message(ImmutableBuffer(datagram.body.data(), datagram.body.size()), *reinterpret_cast<Header*>(datagram.header.data()));
			boost::optional<T> result = message.template cast<T>();
			if(result) {
				return std::make_pair(std::move(result.get()), datagram.remote);
			} else {
				const auto typeId = message.getHeader().typeId;

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
		std::launch::deferred,
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
		std::launch::deferred,
		[datagramFuture = std::move(datagram), visitor = std::forward<Vis>(visitor)]() mutable
			-> typename std::remove_reference_t<Vis>::Result
		{
			auto datagram = datagramFuture.get();
			std::get<Endpoint>(*(visitor.argTuple)) = datagram.remote;
			std::cout << "Message type = " << reinterpret_cast<Header*>(datagram.header.data())->typeId;
			std::cout << "buffer(" << datagram.body.size() << ") =";
			for(unsigned i = 0; i < datagram.body.size(); i++) {
				std::cout << static_cast<unsigned>(datagram.body.data()[i]) << " ";
			}
			std::cout << std::endl;
			Message message(ImmutableBuffer(datagram.body.data(), datagram.body.size()), *reinterpret_cast<Header*>(datagram.header.data()));
			return message.visit(std::forward<Vis>(visitor));
		}
	);

}

} // End of namespace network

} // End of namespace cracen2
