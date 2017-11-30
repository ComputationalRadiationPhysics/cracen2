#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <future>
#include <limits>
#include <cstdint>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Debug.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/CoarseGrainedLocked.hpp"
#include "cracen2/util/AtomicQueue.hpp"

namespace cracen2 {

namespace sockets {

class AsioStreamingSocket {
public:

	using tcp = boost::asio::ip::tcp;
	struct MaxMessageSize {
		static constexpr std::size_t total = std::numeric_limits<std::size_t>::max();
		static constexpr std::size_t body = total;
		static constexpr std::size_t header = total;
	};

	using Endpoint = tcp::endpoint;
	struct Datagram {
		network::Buffer header;
		network::Buffer body;
		Endpoint remote;

		Datagram() = default;
		Datagram(const Datagram&) = delete;
		Datagram(Datagram&&) = default;
	};

	Endpoint local;
	std::mutex mutex;

private:

	using Socket = tcp::socket;
	using Acceptor = tcp::acceptor;

	using ImmutableBuffer = network::ImmutableBuffer;

	boost::asio::io_service io_service;
	util::JoiningThread serviceThread;
	boost::asio::io_service::work work;

	Acceptor acceptor;
	std::function<void(const boost::system::error_code&)> handle_accept(std::shared_ptr<Socket> socket);

	using EndpointSocketMapType = cracen2::util::CoarseGrainedLocked<
		std::map<
			tcp::endpoint,
			Socket
		>
	>;

	using PromiseQueueType = cracen2::util::AtomicQueue<
		std::promise<Datagram>
	>;

	EndpointSocketMapType sockets;
	PromiseQueueType promiseQueue;

	void handle_receive(Socket& socket);
	void handle_datagram(std::shared_ptr<Datagram> d);

public:

	AsioStreamingSocket();
	~AsioStreamingSocket();

	AsioStreamingSocket(AsioStreamingSocket&& other) = default;
	AsioStreamingSocket& operator=(AsioStreamingSocket&& other) = default;

	AsioStreamingSocket(const AsioStreamingSocket& other) = delete;
	AsioStreamingSocket& operator=(const AsioStreamingSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0));

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote, const ImmutableBuffer& header = ImmutableBuffer(nullptr, 0));
	std::future<Datagram> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class Asio AsioStreamingSocket

} // End of namespace sockets

} // End of namespace cracen2
