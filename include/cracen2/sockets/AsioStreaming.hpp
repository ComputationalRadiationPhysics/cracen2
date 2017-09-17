#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Debug.hpp"

namespace cracen2 {

namespace sockets {

class AsioStreamingSocket {
private:
	using tcp = boost::asio::ip::tcp;
	using Socket = tcp::socket;
	using ImmutableBuffer = network::ImmutableBuffer;
	static boost::asio::io_service io_service;

	Socket socket;

public:

	using Endpoint = tcp::endpoint;

	class Acceptor {
	private:
		tcp::acceptor acceptor;

	public:
		Acceptor();
		~Acceptor();
		void bind(Endpoint endpoint);
		void bind();

		AsioStreamingSocket accept();
		Endpoint getLocalEndpoint() const;
	};

	AsioStreamingSocket();
	~AsioStreamingSocket();

	AsioStreamingSocket(AsioStreamingSocket&& other) = default;
	AsioStreamingSocket& operator=(AsioStreamingSocket&& other) = default;

	AsioStreamingSocket(const AsioStreamingSocket& other) = delete;
	AsioStreamingSocket& operator=(const AsioStreamingSocket& other) = delete;


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
