#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/network/Socket.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

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
	void send(const ImmutableBuffer& data);
	size_t probe();
	network::Buffer receive(size_t size);
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void shutdown();
	void close();

}; // End of class Asio AsioStreamingSocket

} // End of namespace sockets

namespace network {

/* defining the traits for the communicator */
template<>
struct IsStreamingSocket<sockets::AsioStreamingSocket> {
	static constexpr bool value = true;
};

#ifdef CRACEN2_ENABLE_EXTERN_TEMPLATES

extern template class Socket<sockets::AsioStreamingSocket, void>;

#endif

} // End of namespace network

} // End of namespace cracen2
