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
	io_service.stop();
}

void AsioStreamingSocket::handle_datagram(std::shared_ptr<Datagram> d) {
	auto promise = promiseQueue.tryPop(std::chrono::milliseconds(0));
	if(promise) {
		promise->set_value(std::move(*d));
	} else {
		io_service.post([this, d]() {
			handle_datagram(std::move(d));
		});
	}
}
void cracen2::sockets::AsioStreamingSocket::handle_receive(Socket& socket) {
	using buffer_size_t = std::remove_const<decltype(ImmutableBuffer::size)>::type;

	auto headerSize = std::make_shared<buffer_size_t>();
	socket.async_receive(
		boost::asio::buffer(headerSize.get(), sizeof(buffer_size_t)),
		[headerSize = headerSize, this, &socket](const boost::system::error_code& error, std::size_t) {
// 			std::cout << "headerSize = " << *headerSize << std::endl;
			if(error != boost::system::errc::success) {
// 				std::cout << error << std::endl;
				return;
			}
			network::Buffer header(*headerSize);
			if(header.size() > 0) boost::asio::read(socket, boost::asio::buffer(header.data(), header.size()));
			buffer_size_t bodySize;
// 			std::cout << "read bodySize" << std::endl;
			boost::asio::read(socket, boost::asio::buffer(&bodySize, sizeof(buffer_size_t)));
			network::Buffer body(bodySize);
// 			std::cout << "bodySize = " << bodySize << std::endl;
			if(bodySize > 0) boost::asio::read(socket, boost::asio::buffer(body.data(),bodySize));

			auto datagram = std::make_shared<Datagram>();
			datagram->header = std::move(header);
			datagram->body = std::move(body);
			datagram->remote = socket.remote_endpoint();
			handle_datagram(std::move(datagram));
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


	io_service.post([&socket, header, data, this, p](){
		try {
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

			boost::asio::write(
				socket,
				buffers
			);
			mutex.unlock();
			p->set_value();
		} catch(...) {
			p->set_exception(std::current_exception());
		}
	});

	return future;
}

std::future<AsioStreamingSocket::Datagram> AsioStreamingSocket::asyncReceiveFrom() {
	std::promise<AsioStreamingSocket::Datagram> promise;
	auto future = promise.get_future();
	promiseQueue.push(std::move(promise));
	return future;
}

bool AsioStreamingSocket::isOpen() const {
	return acceptor.is_open();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getLocalEndpoint() const {
	return acceptor.local_endpoint();
}

void AsioStreamingSocket::close() {
	auto view = sockets.getView();
	(*view)->clear();
}
