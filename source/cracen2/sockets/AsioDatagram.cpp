#include "cracen2/sockets/AsioDatagram.hpp"

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

std::pair<Buffer, AsioDatagramSocket::Endpoint> AsioDatagramSocket::receiveFrom() {
	Endpoint endpoint;
	const size_t messageSize = probe();
	Buffer rawMessage(messageSize);
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

#ifdef CRACEN2_ENABLE_EXTERN_TEMPLATES

namespace cracen2 {

namespace network {

template class Socket<AsioDatagramSocket, void>;

}

}

#endif
