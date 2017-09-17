#include "cracen2/sockets/AsioStreaming.hpp"
#include <limits>

using namespace cracen2::sockets;
using namespace cracen2::network;

constexpr std::uint16_t basePort = 39000;

boost::asio::io_service AsioStreamingSocket::io_service;

AsioStreamingSocket::Acceptor::Acceptor() :
	acceptor(
		io_service
	)
{
	acceptor.open(tcp::v4());
}

AsioStreamingSocket::Acceptor::~Acceptor() {
	if(acceptor.is_open()) {
		acceptor.close();
	}
}

void AsioStreamingSocket::Acceptor::bind(Endpoint endpoint) {
	acceptor.bind(endpoint);
	acceptor.listen();
}

void AsioStreamingSocket::Acceptor::bind() {

	for(auto port = basePort; port < std::numeric_limits<decltype(port)>::max(); port++) {
		try{
			bind(
				Endpoint(
					boost::asio::ip::address::from_string("0.0.0.0"),
					port
				)
			);
			break;
		} catch(const std::exception&) {
		}
	}
	acceptor.listen();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::Acceptor::getLocalEndpoint() const {
	return acceptor.local_endpoint();
}

AsioStreamingSocket AsioStreamingSocket::Acceptor::accept() {

	AsioStreamingSocket socket;
	acceptor.accept(socket.socket);

	return socket;
}

AsioStreamingSocket::AsioStreamingSocket() :
	socket(io_service)
{
}

AsioStreamingSocket::~AsioStreamingSocket()
{
	if(socket.is_open()) {
		socket.close();
	}
}

void AsioStreamingSocket::connect(Endpoint destination) {
	socket.connect(destination);
}

void AsioStreamingSocket::send(const ImmutableBuffer& data) {
	boost::asio::write(
		socket,
		boost::asio::buffer(
			reinterpret_cast<const void*>(&data.size),
			sizeof(data.size)
		),
		boost::asio::transfer_all()
	);
	boost::asio::write(
		socket,
		boost::asio::buffer(
			data.data,
			data.size
		),
		boost::asio::transfer_all()
	);
}

Buffer AsioStreamingSocket::receive() {
	using SizeType = std::remove_cv<decltype(ImmutableBuffer::size)>::type;
	SizeType messageSize = 0;

	boost::asio::read(socket, boost::asio::buffer(reinterpret_cast<void*>(&messageSize), sizeof(SizeType)));

	Buffer buffer(messageSize);
	boost::asio::read(socket, boost::asio::buffer(buffer.data(), messageSize));
	return buffer;
}

bool AsioStreamingSocket::isOpen() const {
	return socket.is_open();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getLocalEndpoint() const {
	return socket.local_endpoint();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getRemoteEndpoint() const {
	return socket.remote_endpoint();
}

void AsioStreamingSocket::shutdown() {
	socket.shutdown(Socket::shutdown_type::shutdown_both);
}

void AsioStreamingSocket::close() {
	socket.close();
}
