#include "cracen2/sockets/AsioDatagram.hpp"

using namespace cracen2::sockets;
using namespace cracen2::network;
using namespace cracen2::util;

AsioDatagramSocket::AsioDatagramSocket() :
	work(io_service),
	socket(io_service)
{
	serviceThread = JoiningThread("AsioDatagramSocket::ServiceThread",[this](){ io_service.run(); });
	socket.open(udp::v4());
	boost::asio::socket_base::receive_buffer_size option1(256*1024*1024);
	boost::asio::socket_base::send_buffer_size option2(256*1024*1024);

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

std::future<void> AsioDatagramSocket::asyncSendTo(const ImmutableBuffer& data, const Endpoint remote, const ImmutableBuffer& header) {
	auto promise = std::make_shared<std::promise<void>>();
	std::vector<boost::asio::const_buffer> buffers;
	buffers.reserve(3);
	buffers.push_back(boost::asio::buffer(data.data, data.size));
	buffers.push_back(boost::asio::buffer(header.data, header.size));
	buffers.push_back(boost::asio::buffer(&header.size, sizeof(header.size)));

// 	Buffer b(data.size + header.size + sizeof(header.size));
// 	std::memcpy(b.data(), data.data, data.size);
// 	std::memcpy(b.data() + data.size, header.data, header.size);
// 	std::memcpy(b.data() + data.size + header.size, &header.size, sizeof(header.size));

	socket.async_send_to(
		buffers,
		remote,
		[promise](const boost::system::error_code& error, std::size_t) {
			if(error != boost::system::errc::success) throw std::runtime_error(error.message());
			promise->set_value();
		}
	);
	return promise->get_future();
}

std::future<AsioDatagramSocket::Datagram> AsioDatagramSocket::asyncReceiveFrom() {
	static constexpr std::size_t maxFrameSize = std::numeric_limits<std::uint16_t>::max();

	// Probe first
	auto remote = std::make_shared<Endpoint>();
	auto promise = std::make_shared<std::promise<Datagram>>();
	auto buffer = std::make_shared<Buffer>(maxFrameSize);

	socket.async_receive_from(
		boost::asio::buffer(buffer->data(), buffer->size()),
		*remote,
		[promise, buffer, remote](const boost::system::error_code& error, std::size_t received) {

			if(error != boost::system::errc::success) throw std::runtime_error(error.message());
			buffer->shrink(received);
			// Message is in buffer
			Datagram d;
			d.remote = std::move(*remote);

			const auto headerSize = *reinterpret_cast<decltype(ImmutableBuffer::size)*>(buffer->data() + buffer->size() - sizeof(ImmutableBuffer::size));
			d.header = Buffer(headerSize);
			std::memcpy(d.header.data(), buffer->data() + buffer->size() - sizeof(headerSize) - headerSize, headerSize);

			const auto bodySize = buffer->size() - headerSize - sizeof(headerSize);
			buffer->shrink(bodySize);
			d.body = std::move(*buffer);

			promise->set_value(std::move(d));
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
