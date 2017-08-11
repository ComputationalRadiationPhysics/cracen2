#pragma once

#include "Message.hpp"
#include "Socket.hpp"

namespace cracen2 {

namespace network {

/*
 * The Communicator combines the abstraction of the network::Socket and network::Message
 */
template<class SocketImplementation, class TagList>
class Communicator {
public:

	using Message = cracen2::network::Message<TagList>;
	using Visitor = typename Message::Visitor;
	using Socket = cracen2::network::Socket<SocketImplementation>;
	using Endpoint = typename Socket::Endpoint;
	using Port = typename Socket::Port;

private:

	Socket socket;

public:

	Communicator();

	void bind(const Port& port);
	void accept();

	void connect(const Endpoint& destination);

	template <class T>
	void send(const T& data);

	// This has to be used with extreme caution. Guessing the wrong type will cause packages to be droped
	template <class T>
	T receive();

	void receive(Visitor visitor);


	//Socket Operations
	bool isOpen();
	Endpoint getLocalEndpoint();
	Endpoint getRemoteEndpoint();

	void close();

}; // End of class Communicator

template <class SocketImplementation, class TagList>
Communicator<SocketImplementation, TagList>::Communicator() {

}

template <class SocketImplementation, class TagList>
void Communicator<SocketImplementation, TagList>::bind(const Port& port) {
	socket.bind(port);
}

template <class SocketImplementation, class TagList>
void Communicator<SocketImplementation, TagList>::accept() {
	socket.accept();
}

template <class SocketImplementation, class TagList>
void Communicator<SocketImplementation, TagList>::connect(const Endpoint& destination) {
	socket.connect(destination);
}

template <class SocketImplementation, class TagList>
template <class T>
void Communicator<SocketImplementation, TagList>::send(const T& data) {
	Message message(data);
	socket.send(ImmutableBuffer(message.getBuffer().data(), message.getBuffer().size()));
}

template <class SocketImplementation, class TagList>
template <class T>
T Communicator<SocketImplementation, TagList>::receive() {
	Message message(socket.receive());

	boost::optional<T> result = message.template cast<T>();
	if(result) {
		return std::move(result.get());
	} else {
		throw(std::runtime_error("Trying to receive a message with a wrong type."));
	}
}

template <class SocketImplementation, class TagList>
void Communicator<SocketImplementation, TagList>::receive(Visitor visitor) {
	Message message(socket.receive());

	message.visit(visitor);
}

template <class SocketImplementation, class TagList>
bool Communicator<SocketImplementation, TagList>::isOpen() {
	return socket.isOpen();
}

template <class SocketImplementation, class TagList>
typename Communicator<SocketImplementation, TagList>::Endpoint Communicator<SocketImplementation, TagList>::getLocalEndpoint() {
	return socket.getLocalEndpoint();
}

template <class SocketImplementation, class TagList>
typename Communicator<SocketImplementation, TagList>::Endpoint Communicator<SocketImplementation, TagList>::getRemoteEndpoint() {
	return socket.getRemoteEndpoint();
}

template <class SocketImplementation, class TagList>
void Communicator<SocketImplementation, TagList>::close() {
	socket.close();
}

} // End of namespace network

} // End of namespace cracen2
