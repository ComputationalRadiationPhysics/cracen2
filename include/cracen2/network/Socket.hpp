#pragma once

#include <type_traits>
#include "ImmutableBuffer.hpp"

namespace cracen2 {

namespace network {

template <class Socket>
struct IsDatagramSocket;

template <class Socket>
struct IsDatagramSocket
{
	static constexpr bool value = false;
};

template <class Socket>
struct IsStreamingSocket;

template <class Socket>
struct IsStreamingSocket {
	static constexpr bool value = false;
};

/*
 * The Socket class provides a unified interface for both streaming and datagram sockets
 * for the higher abstraction layer
 */
template <class SocketImplementation, class enable = void>
class Socket;

template <class SocketImplementation>
class Socket <
	SocketImplementation,
	typename std::enable_if<
			IsDatagramSocket<SocketImplementation>::value
	>::type
>
{
public:

	using Endpoint = typename SocketImplementation::Endpoint;
	using Port = typename SocketImplementation::Port;

private:

	using DatagramSocket = SocketImplementation;
	DatagramSocket socket;
	Endpoint destination;

public:

	bool isOpen() const {
		return socket.isOpen();
	}

	Endpoint getLocalEndpoint() const {
		return socket.getLocalEndpoint();
	}

	Endpoint getRemoteEndpoint() const {
		return destination;
	}

	void bind(const Port& port) {
		socket.bind(port);
	}

	void accept() {
	}

	void connect(const Endpoint& destination) {
		this->destination = destination;
	}
	void send(const ImmutableBuffer& buffer) {
		socket.sendTo(destination, buffer);
	}

	Buffer receive() {
		const auto sender = socket.receiveFrom();
		connect(sender.second);
		return sender.first;
	}

	void close() {
		socket.close();
	}

}; // End of class Socket


template <class SocketImplementation>
class Socket <
	SocketImplementation,
	typename std::enable_if<
		IsStreamingSocket<SocketImplementation>::value
	>::type
>
{
public:

	using Endpoint = typename SocketImplementation::Endpoint;
	using Port = typename SocketImplementation::Port;
	using MessageSizeType = size_t;

private:

	using StreamingSocket = SocketImplementation;
	StreamingSocket socket;
	Endpoint sender;

public:

	bool isOpen() const {
		return socket.isOpen();
	}

	Endpoint getLocalEndpoint() const {
		return socket.getLocalEndpoint();
	}

	Endpoint getRemoteEndpoint() const {
		return socket.getRemoteEndpoint();
	}

	void bind(const Port& port) {
		socket.bind(port);
	}

	void accept() {
		sender = socket.accept();
	}

	void connect(const Endpoint& destination) {
		socket.connect(destination);
	}

	void send(const ImmutableBuffer& buffer) {
		const MessageSizeType messageSize = buffer.size;
		ImmutableBuffer messageSizeHeader(reinterpret_cast<const std::uint8_t*>(&messageSize), sizeof(messageSize));
		socket.send(messageSizeHeader);
		socket.send(buffer);
	}

	Buffer receive() {
		Buffer sizeBuffer = socket.receive(sizeof(MessageSizeType));
		if(sizeBuffer.size() < sizeof(MessageSizeType)) throw std::runtime_error("Remote socket closed connection.");
		Buffer data = socket.receive(*reinterpret_cast<MessageSizeType*>(sizeBuffer.data()));
		return data;
	}

	void close() {
		socket.close();
	}

}; // End of class Socket

} // End of namespace network

} // End of namespace cracen2
