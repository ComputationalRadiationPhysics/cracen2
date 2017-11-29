#include "cracen2/sockets/AsioStreaming.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;


using Endpoint = AsioStreamingSocket::Endpoint;
using Socket = boost::asio::ip::tcp::socket;

AsioStreamingSocket::AsioStreamingSocket() :
	io_service(),
	work(io_service),
	acceptor(io_service),
	promiseQueue(200)
{
	if(!serviceThread.joinable()) {
		serviceThread = JoiningThread("AsioStreamingSocket::ServiceThread", [this](){
			io_service.run();
		});
	}
}

AsioStreamingSocket::~AsioStreamingSocket() {

}

void cracen2::sockets::AsioStreamingSocket::handle_receive(Socket& socket) {
	using buffer_size_t = std::remove_const<decltype(ImmutableBuffer::size)>::type;

	auto headerSize = std::make_shared<buffer_size_t>();
	socket.async_receive(
		boost::asio::buffer(headerSize.get(), sizeof(buffer_size_t)),
		[headerSize = std::move(headerSize), this, &socket](const boost::system::error_code& error, std::size_t){
// 			std::cout << "headerSize = " << *headerSize << " read = " << s << std::endl;
			if(error != boost::system::errc::success) {
				return;
			}
			network::Buffer header(*headerSize);
			boost::asio::read(socket, boost::asio::buffer(header.data(), header.size()));
			buffer_size_t bodySize;
			boost::asio::read(socket, boost::asio::buffer(&bodySize, sizeof(buffer_size_t)));
			network::Buffer body(bodySize);
// 			std::cout << "bodySize = " << bodySize << std::endl;
			boost::asio::read(socket, boost::asio::buffer(body.data(),bodySize));

			Datagram datagram;
			datagram.header = std::move(header);
			datagram.body = std::move(body);
			datagram.remote = socket.remote_endpoint();
			auto promise = promiseQueue.pop();
			promise.set_value(
				std::move(datagram)
			);

			handle_receive(socket);
		}
	);
}


std::function<void(const boost::system::error_code&)> AsioStreamingSocket::handle_accept(std::shared_ptr<Socket> socket) {
	return [socket, this](const boost::system::error_code& error) {
		if(error == boost::system::errc::operation_canceled) {
			return;
		};
		if(error != boost::system::errc::success) {
			std::cerr << error << std::endl;
			throw error;
		}
		auto viewPtr = sockets.getView();
		auto& view = *viewPtr;
		const auto remote = socket->remote_endpoint();
		view->insert(std::make_pair(remote, std::move(*socket)));
		handle_receive(view->at(remote));

		auto s2 = std::make_shared<Socket>(io_service);
		acceptor.async_accept(
			*s2,
			handle_accept(s2)
		);
	};
}

void AsioStreamingSocket::bind(Endpoint endpoint) {
	acceptor.open(endpoint.protocol());
	acceptor.bind(endpoint);
	acceptor.listen();
	auto socket = std::make_shared<Socket>(io_service);
	acceptor.async_accept(
		*socket,
		handle_accept(socket)
	);
}

std::future<void> AsioStreamingSocket::asyncSendTo(
	const ImmutableBuffer& data,
	const Endpoint remote,
	const ImmutableBuffer& header
) {
	auto p = std::make_shared<std::promise<void>>();
	auto future = p->get_future();
	auto view = sockets.getView();
	if((*view)->count(remote) == 0) {
		Socket s(io_service);
		s.open(local.protocol());
		s.bind(local);
		s.connect(remote);
		(*view)->insert(std::make_pair(remote, std::move(s)));
		handle_receive((*view)->at(remote));
	}
	auto& socket = (*view)->at(remote);

	std::vector<boost::asio::const_buffer> buffers {
		boost::asio::buffer(&header.size, sizeof(header.size)),
		boost::asio::buffer(header.data, header.size),
		boost::asio::buffer(&data.size, sizeof(data.size)),
		boost::asio::buffer(data.data, data.size),
	};

	mutex.lock();
	// According to this Post https://stackoverflow.com/questions/7362894/boostasiosocket-thread-safety
	// multiple async_write_some function can be interleaved against each other, which will break
	// the integrety of the stream.

	boost::asio::async_write(
		socket,
		buffers,
		[p, this](const boost::system::error_code&, std::size_t) {
			mutex.unlock();
			p->set_value();
		}
	);

	return future;
}

std::future<AsioStreamingSocket::Datagram> AsioStreamingSocket::asyncReceiveFrom() {
	std::promise<AsioStreamingSocket::Datagram> promise;
	auto future = promise.get_future();
	promiseQueue.push(std::move(promise));
	return future;
}

bool AsioStreamingSocket::isOpen() const {
	auto view = sockets.getReadOnlyView();
	if( (*view)->size() > 0) {
		return 1;
	}
	return 0;
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getLocalEndpoint() const {
	return acceptor.local_endpoint();
}

void AsioStreamingSocket::close() {
	auto view = sockets.getView();
	(*view)->clear();
}
