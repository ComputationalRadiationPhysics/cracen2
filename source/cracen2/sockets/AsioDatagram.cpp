#include "cracen2/sockets/AsioDatagram.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;

boost::asio::io_service AsioDatagramSocket::io_service;


AsioDatagramSocket::Acceptor::Acceptor() = default;
AsioDatagramSocket::Acceptor::~Acceptor() = default;

void AsioDatagramSocket::Acceptor::bind(Endpoint endpoint) {
	socket = std::unique_ptr<AsioDatagramSocket>( new AsioDatagramSocket() );
	socket->socket.bind(endpoint);
	this->endpoint = socket->getLocalEndpoint();
}

AsioDatagramSocket AsioDatagramSocket::Acceptor::accept() {
	if(socket == nullptr) throw std::runtime_error("It is only possible to accept one datagram socket.");
	return std::move(*(socket.release()));
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::Acceptor::getLocalEndpoint() const {
	return endpoint;
}

bool AsioDatagramSocket::Acceptor::isOpen() const {
	return socket != nullptr;
}

void AsioDatagramSocket::Acceptor::close() {
	socket.reset();
}

AsioDatagramSocket::AsioDatagramSocket() :
	socket(io_service)
{
	socket.open(udp::v4());
}

AsioDatagramSocket::~AsioDatagramSocket()
{
	if(socket.is_open()) {
		socket.close();
	}
}

void AsioDatagramSocket::connect(AsioDatagramSocket::Endpoint endpoint) {
	remote = endpoint;
}

void AsioDatagramSocket::send(const ImmutableBuffer& data) {
	socket.send_to(boost::asio::buffer(data.data, data.size), remote);
}

size_t AsioDatagramSocket::probe() {
	socket.receive(boost::asio::null_buffers());
	return socket.available();
}

Buffer AsioDatagramSocket::receive() {
	const size_t messageSize = probe();
	Buffer rawMessage(messageSize);
	boost::asio::ip::udp::endpoint sender_endpoint;
	socket.receive_from(
		boost::asio::buffer(
			rawMessage.data(),
			rawMessage.size()
		),
		remote
	);
	return rawMessage;
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::getLocalEndpoint() const {
	return socket.local_endpoint();
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::getRemoteEndpoint() const {
	return remote;
}

bool AsioDatagramSocket::isOpen() const {
	return socket.is_open();
}

void AsioDatagramSocket::close() {
	socket.close();
}
