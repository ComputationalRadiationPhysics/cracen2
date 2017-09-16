#include "cracen2/sockets/AsioStreaming.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;

AsioStreamingSocket::AsioStreamingSocket(int ipProtocol) :
	socket(
		io_service
	),
	acceptor(
		io_service,
		ipProtocol == 6 ? tcp::v6() : tcp::v4()
	)
{

}

AsioStreamingSocket::~AsioStreamingSocket()
{
	if(acceptor.is_open()) {
		acceptor.close();
	}
	if(socket.is_open()) {
		socket.close();
	}
}

void AsioStreamingSocket::bind(Port port) {
	const auto protocol = acceptor.local_endpoint().protocol();
	acceptor.bind(
		Endpoint(
			protocol,
			port
		)
	);
	acceptor.listen();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::accept() {
	//shutdown();
	socket.close();

	Endpoint endpoint;
	acceptor.accept(socket, endpoint);
	return endpoint;
}

void AsioStreamingSocket::connect(Endpoint destination) {
	socket.connect(destination);
}

void AsioStreamingSocket::send(const ImmutableBuffer& data) {
	boost::asio::write(
		socket,
		boost::asio::buffer(
			data.data,
			data.size
		),
		boost::asio::transfer_all()
	);
}

Buffer AsioStreamingSocket::receive(size_t size) {
	Buffer buffer(size);
	boost::asio::read(socket, boost::asio::buffer(buffer.data(), size));
	return buffer;
}

bool AsioStreamingSocket::isOpen() const {
	return acceptor.is_open();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getLocalEndpoint() const {
	if(acceptor.is_open()) {
		return acceptor.local_endpoint();
	} else {
		return socket.local_endpoint();
	}
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

#ifdef CRACEN2_ENABLE_EXTERN_TEMPLATES

namespace cracen2 {

namespace network {

template class Socket<AsioStreamingSocket, void>;

}

}

#endif
