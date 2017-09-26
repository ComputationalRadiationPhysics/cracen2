#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "cracen2/util/Thread.hpp"
#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

class AsioStreamingSocket {
private:
	using tcp = boost::asio::ip::tcp;
	using Socket = tcp::socket;
	using Acceptor = tcp::acceptor;
	using ImmutableBuffer = network::ImmutableBuffer;
	boost::asio::io_service io_service;

	bool done;
	std::atomic<bool> closed;
	bool acceptorRunning;
	Acceptor acceptor;
	tcp::endpoint active;
	std::mutex socketMutex;
	std::condition_variable socketConditionVariable;
	using SocketMapType = std::map<tcp::endpoint, std::shared_ptr<Socket>>;
	SocketMapType sockets;

	using SizeType = std::remove_cv<decltype(ImmutableBuffer::size)>::type;
	network::Buffer messageBuffer;

	void receiveHandler(std::shared_ptr<Socket> socket, std::shared_ptr<SizeType>, const boost::system::error_code& error, std::size_t received);

	util::JoiningThread acceptorThread;

public:

	using Endpoint = tcp::endpoint;

	AsioStreamingSocket();
	~AsioStreamingSocket();

	AsioStreamingSocket(AsioStreamingSocket&& other) = default;
	AsioStreamingSocket& operator=(AsioStreamingSocket&& other) = default;

	AsioStreamingSocket(const AsioStreamingSocket& other) = delete;
	AsioStreamingSocket& operator=(const AsioStreamingSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0));
	void accept();
	void connect(Endpoint destination);
	void send(const ImmutableBuffer& data);
	network::Buffer receive();
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void shutdown();
	void close();

}; // End of class Asio AsioStreamingSocket

} // End of namespace sockets

} // End of namespace cracen2
