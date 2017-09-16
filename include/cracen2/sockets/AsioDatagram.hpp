#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/network/Socket.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

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
	std::pair<network::Buffer, AsioDatagramSocket::Endpoint> receiveFrom();
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void close();

}; // End of class Asio AsioDatagramSocket

} // End of namespace sockets

namespace network {

/* defining the traits for the communicator */
template<>
struct IsDatagramSocket<sockets::AsioDatagramSocket> {
	static constexpr bool value = true;
};

#ifdef CRACEN2_ENABLE_EXTERN_TEMPLATES

extern template class cracen2::network::Socket<sockets::AsioDatagramSocket, void>;

#endif

} // End of namespace network

} // End of namespace cracen2
