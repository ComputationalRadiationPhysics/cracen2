#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <future>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Debug.hpp"
#include "cracen2/util/Thread.hpp"

namespace cracen2 {

namespace sockets {

class AsioDatagramSocket {
private:
	using udp = boost::asio::ip::udp;
	using Socket = udp::socket;
	using ImmutableBuffer = network::ImmutableBuffer;

	boost::asio::io_service io_service;
	util::JoiningThread serviceThread;
	boost::asio::io_service::work work;

	Socket socket;

public:

	using Endpoint = udp::endpoint;

	AsioDatagramSocket();
	~AsioDatagramSocket();

	AsioDatagramSocket(AsioDatagramSocket&& other) = default;
	AsioDatagramSocket& operator=(AsioDatagramSocket&& other) = default;

	AsioDatagramSocket(const AsioDatagramSocket& other) = delete;
	AsioDatagramSocket& operator=(const AsioDatagramSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0));

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote);
	std::future<std::pair<network::Buffer, Endpoint>> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class Asio AsioDatagramSocket

} // End of namespace sockets

} // End of namespace cracen2
