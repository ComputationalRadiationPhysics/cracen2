#include "cracen2/sockets/Asio.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;

AsioDatagramSocket::AsioDatagramSocket(int ipProtocol) :
	socket(
		io_service,
		ipProtocol == 6 ? udp::v6() : udp::v4()
	)
{

}

AsioDatagramSocket::~AsioDatagramSocket()
{
	if(socket.is_open()) {
		socket.close();
	}
}

void AsioDatagramSocket::bind(const Port& port) {
	socket.bind(
		Endpoint(socket.local_endpoint().protocol(), port)
	);
}

void AsioDatagramSocket::sendTo(const Endpoint& destination, const ImmutableBuffer& data) {
	socket.send_to(boost::asio::buffer(data.data, data.size), destination);
}

size_t AsioDatagramSocket::probe() {
	socket.receive(boost::asio::null_buffers());
	return socket.available();
}

std::pair<std::vector<std::uint8_t>, AsioDatagramSocket::Endpoint> AsioDatagramSocket::receiveFrom() {
	Endpoint endpoint;
	const size_t messageSize = probe();
	std::vector<std::uint8_t> rawMessage(messageSize);
	boost::asio::ip::udp::endpoint sender_endpoint;
	socket.receive_from(
		boost::asio::buffer(
			rawMessage.data(),
			rawMessage.size()
		),
		endpoint
	);
	return std::make_pair(rawMessage, endpoint);
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::getLocalEndpoint() const {
	return socket.local_endpoint();
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::getRemoteEndpoint() const {
	return socket.remote_endpoint();
}

bool AsioDatagramSocket::isOpen() const {
	return socket.is_open();
}

void AsioDatagramSocket::close() {
	socket.close();
}

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

void AsioStreamingSocket::send(ImmutableBuffer data) {
	boost::asio::write(
		socket,
		boost::asio::buffer(
			data.data,
			data.size
		),
		boost::asio::transfer_all()
	);
}

std::vector<std::uint8_t> AsioStreamingSocket::receive(size_t size) {
	std::vector<std::uint8_t> rawMessage(size);
	socket.receive(
		boost::asio::buffer(
			rawMessage.data(),
			rawMessage.size()
		)
	);
	return rawMessage;
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

template class Socket<AsioDatagramSocket, void>;
template class Socket<AsioStreamingSocket, void>;

}

}

#endif
