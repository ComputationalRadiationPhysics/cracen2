#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "cracen2/network/Socket.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

enum class AsioProtocol {
	tcp,
	udp
};

class AsioDatagramSocket {
private:
	using udp = boost::asio::ip::udp;
	using Socket = udp::socket;
	using ImmutableBuffer = network::ImmutableBuffer;

	boost::asio::io_service io_service;
	Socket socket;

public:

	using Endpoint = udp::endpoint;
	using Port = decltype(Endpoint().port());


	AsioDatagramSocket(int ipProtocol = 4);
	~AsioDatagramSocket();


	void bind(const Port& port);
	void sendTo(const Endpoint& destination, const ImmutableBuffer& data);
	size_t probe();
	std::pair<std::vector<std::uint8_t>, AsioDatagramSocket::Endpoint> receiveFrom();
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void close();

}; // End of class Asio AsioDatagramSocket


class AsioStreamingSocket {
private:
	using tcp = boost::asio::ip::tcp;
	using Socket = tcp::socket;
	using ImmutableBuffer = network::ImmutableBuffer;

	boost::asio::io_service io_service;
	Socket socket;
	tcp::acceptor acceptor;

public:

	using Endpoint = tcp::endpoint;
	using Port = decltype(Endpoint().port());


	AsioStreamingSocket(int ipProtocol = 4);
	~AsioStreamingSocket();

	void bind(Port port);
	Endpoint accept();
	void connect(Endpoint destination);
	void send(ImmutableBuffer data);
	size_t probe();
	std::vector<std::uint8_t> receive(size_t size);
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void shutdown();
	void close();

}; // End of class Asio AsioStreamingSocket


/* Defining traits for the AsioSocket proxy */
template <AsioProtocol protocol>
struct AsioProtocolTrait;

template <>
struct AsioProtocolTrait<AsioProtocol::udp> {
	using type = AsioDatagramSocket;
};

template <>
struct AsioProtocolTrait<AsioProtocol::tcp> {
	using type = AsioStreamingSocket;
};

/* Proxy for tcp and udp implementation */
template <AsioProtocol protocol>
using AsioSocket = typename AsioProtocolTrait<protocol>::type;

} // End of namespace sockets

namespace network {

/* defining the traits for the communicator */
template<>
struct IsDatagramSocket<sockets::AsioSocket<sockets::AsioProtocol::udp>> {
	static constexpr bool value = true;
};

/* defining the traits for the communicator */
template<>
struct IsStreamingSocket<sockets::AsioSocket<sockets::AsioProtocol::tcp>> {
	static constexpr bool value = true;
};

#ifdef CRACEN2_ENABLE_EXTERN_TEMPLATES

template <class SocketImplementation, class enable>
class Socket;

extern template class Socket<sockets::AsioDatagramSocket, void>;
extern template class Socket<sockets::AsioStreamingSocket, void>;

#endif

} // End of namespace network

} // End of namespace cracen2
