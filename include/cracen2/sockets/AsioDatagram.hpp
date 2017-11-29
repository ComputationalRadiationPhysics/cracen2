#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <future>
#include <limits>
#include <cstdint>

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

	struct MaxMessageSize {
		static constexpr std::size_t total = std::numeric_limits<std::uint16_t>::max() - sizeof(ImmutableBuffer::size);
		static constexpr std::size_t body = total;
		static constexpr std::size_t header = total;
	};

	using Endpoint = udp::endpoint;
	struct Datagram {
		network::Buffer header;
		network::Buffer body;
		Endpoint remote;
	};

	AsioDatagramSocket();
	~AsioDatagramSocket();

	AsioDatagramSocket(AsioDatagramSocket&& other) = default;
	AsioDatagramSocket& operator=(AsioDatagramSocket&& other) = default;

	AsioDatagramSocket(const AsioDatagramSocket& other) = delete;
	AsioDatagramSocket& operator=(const AsioDatagramSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0));

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote, const ImmutableBuffer& header = ImmutableBuffer(nullptr, 0));
	std::future<Datagram> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class Asio AsioDatagramSocket

} // End of namespace sockets

} // End of namespace cracen2
