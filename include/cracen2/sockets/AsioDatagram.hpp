#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

class AsioDatagramSocket {
private:
	using udp = boost::asio::ip::udp;
	using Socket = udp::socket;
	using ImmutableBuffer = network::ImmutableBuffer;

	static boost::asio::io_service io_service;
	Socket socket;

	udp::endpoint remote;

	size_t probe();

public:

	using Endpoint = udp::endpoint;

	AsioDatagramSocket();
	~AsioDatagramSocket();

	AsioDatagramSocket(AsioDatagramSocket&& other) = default;
	AsioDatagramSocket& operator=(AsioDatagramSocket&& other) = default;

	AsioDatagramSocket(const AsioDatagramSocket& other) = delete;
	AsioDatagramSocket& operator=(const AsioDatagramSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0));
	void accept();
	void connect(Endpoint destination);
	void send(const ImmutableBuffer& data);
	network::Buffer receive();
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void close();

}; // End of class Asio AsioDatagramSocket

} // End of namespace sockets

} // End of namespace cracen2
