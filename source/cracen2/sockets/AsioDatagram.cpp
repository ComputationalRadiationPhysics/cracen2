#include "cracen2/sockets/AsioDatagram.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;
using namespace cracen2::util;

AsioDatagramSocket::AsioDatagramSocket() :
	work(io_service),
	socket(io_service)
{
	serviceThread = JoiningThread([this](){ io_service.run(); });
	socket.open(udp::v4());
	boost::asio::socket_base::receive_buffer_size option1(128*1024*1024);
	boost::asio::socket_base::send_buffer_size option2(128*1024*1024);

	socket.set_option(option1);
	socket.set_option(option2);

}

AsioDatagramSocket::~AsioDatagramSocket()
{
	if(socket.is_open()) {
		socket.close();
	}
}

void AsioDatagramSocket::bind(Endpoint local) {
	socket.bind(local);
}

std::future<void> AsioDatagramSocket::asyncSendTo(const ImmutableBuffer& data, const Endpoint remote) {
	auto promise = std::make_shared<std::promise<void>>();
	socket.async_send_to(
		boost::asio::buffer(data.data, data.size),
		remote,
		[promise](const boost::system::error_code& error, std::size_t) {
			if(error != boost::system::errc::success) throw std::runtime_error(error.message());
			promise->set_value();
		}
	);
	return promise->get_future();
}

std::future<std::pair<Buffer, AsioDatagramSocket::Endpoint>> AsioDatagramSocket::asyncReceiveFrom() {
	static constexpr std::size_t maxFrameSize = std::numeric_limits<std::uint16_t>::max();

	// Probe first
	auto remote = std::make_shared<Endpoint>();
	auto promise = std::make_shared<std::promise<std::pair<Buffer, Endpoint>>>();
	auto buffer = std::make_shared<Buffer>(maxFrameSize);

	socket.async_receive_from(
		boost::asio::buffer(buffer->data(), buffer->size()),
		*remote,
		[promise, buffer, remote](const boost::system::error_code& error, std::size_t received) {
			if(error != boost::system::errc::success) throw std::runtime_error(error.message());
			buffer->shrink(received);
			// Message is in buffer
			promise->set_value(std::make_pair(std::move(*buffer), *remote));
		}
	);

	return promise->get_future();
}

AsioDatagramSocket::Endpoint AsioDatagramSocket::getLocalEndpoint() const {
	return socket.local_endpoint();
}

bool AsioDatagramSocket::isOpen() const {
	return socket.is_open();
}

void AsioDatagramSocket::close() {
	socket.close();
}
