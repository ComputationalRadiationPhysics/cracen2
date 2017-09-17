#include "cracen2/sockets/AsioDatagram.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;

boost::asio::io_service AsioDatagramSocket::io_service;


AsioDatagramSocket::Acceptor::Acceptor() = default;
AsioDatagramSocket::Acceptor::~Acceptor() = default;

void AsioDatagramSocket::Acceptor::bind(Endpoint endpoint) {
	local = endpoint;
}

void AsioDatagramSocket::Acceptor::bind() {
	local = Endpoint(
		boost::asio::ip::address::from_string("0.0.0.0"),
		39000
	);
}

AsioDatagramSocket AsioDatagramSocket::Acceptor::accept() {
	AsioDatagramSocket socket;
	socket.socket.bind(local);
	return socket;
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::Acceptor::getLocalEndpoint() const {
	return local;
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
	return socket.remote_endpoint();
}

bool AsioDatagramSocket::isOpen() const {
	return socket.is_open();
}

void AsioDatagramSocket::close() {
	socket.close();
}
