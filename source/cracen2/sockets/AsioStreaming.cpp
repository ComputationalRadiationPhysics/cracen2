#include "cracen2/sockets/AsioStreaming.hpp"
#include <limits>
#include <functional>
#include <future>

using namespace cracen2::sockets;
using namespace cracen2::network;

void AsioStreamingSocket::bind(Endpoint endpoint) {
	acceptor.bind(endpoint);
	acceptor.listen();
}

void AsioStreamingSocket::accept() {
	acceptorRunning = true;
	acceptorThread = util::JoiningThread([this](){
		try{
			while(acceptorRunning) {
				auto socket = std::make_shared<Socket>(io_service);
				acceptor.accept(*socket);
				auto messageSize = std::make_shared<SizeType>();
				boost::asio::async_read(
					*socket, boost::asio::buffer(
						messageSize.get(),
						sizeof(SizeType)
					),
					boost::asio::transfer_at_least(sizeof(SizeType)),
					[messageSize, socket, this](const boost::system::error_code& error, std::size_t received){
						receiveHandler(socket, std::move(messageSize), error, received);
					}
				);
			 	std::unique_lock<std::mutex> lock(socketMutex);
				if(active == Endpoint()) {
					active = socket->remote_endpoint();
				}
 				sockets.insert(std::make_pair(socket->remote_endpoint(), socket));
				socketConditionVariable.notify_one();
			}
		} catch (const std::exception& e) {
			std::cerr << "TcpSocket Acceptor catched exception:" << e.what() << std::endl;
		}
	});
}

AsioStreamingSocket::AsioStreamingSocket() :
	closed(false),
	acceptorRunning(false),
	acceptor(io_service)
{
	acceptor.open(tcp::v4());
}

AsioStreamingSocket::~AsioStreamingSocket()
{
	close();
}

void AsioStreamingSocket::receiveHandler(
	std::shared_ptr<Socket> socket,
	std::shared_ptr<SizeType> messageSize,
	const boost::system::error_code& error,
	std::size_t received
) {
	io_service.stop();
	if(error != boost::system::errc::success) {
		std::stringstream s;
		s << "AsioStreamingSocket: Boost threw error value = " << error;
		throw std::runtime_error(s.str());
		//sockets.erase(sockets.find(socket->remote_endpoint()));
		//return;
	};
	if(received != sizeof(SizeType)) throw std::runtime_error("AsioStreamingSocket: Header of message is incomplete.");
	active = socket->remote_endpoint();

//	std::cout << "message (from " << active << ") size = " << messageSize << std::endl;
	messageBuffer.resize(*messageSize);
	auto size = boost::asio::read(
		*socket,
		boost::asio::buffer(
			messageBuffer.data(),
			*messageSize
		)
	); // Read whole message

	if(size != *messageSize) {
		std::stringstream s;
		s<< "Read only " << size << " of " << messageSize << " Bytes" << std::endl;
		std::cerr << s.str() << std::endl;
		throw std::runtime_error(s.str());
	}
	done = true;
	boost::asio::async_read( // push the async read for the next round to the io_service
		*socket,
		boost::asio::buffer(&messageSize,sizeof(SizeType)),
		boost::asio::transfer_at_least(sizeof(SizeType)),
		[this, socket, messageSize](const boost::system::error_code& error, std::size_t received){
			receiveHandler(std::move(socket), std::move(messageSize), error, received);
		}
	);
}

void AsioStreamingSocket::connect(Endpoint destination) {
	std::unique_lock<std::mutex> lock(socketMutex);
	if(sockets.count(destination) == 0) {
		auto socket = std::make_shared<Socket>(io_service, boost::asio::ip::tcp::v4());
		//socket.open(boost::asio::ip::tcp::v4());
		socket->bind(
			Endpoint(
				boost::asio::ip::address::from_string("0.0.0.0"),
				0
			)
		);
		socket->connect(destination);
		auto messageSize = std::make_shared<SizeType>();
		boost::asio::async_read(
			*socket, boost::asio::buffer(
				messageSize.get(),
				sizeof(SizeType)
			),
			boost::asio::transfer_at_least(sizeof(SizeType)),
			[this, socket, messageSize](const boost::system::error_code& error, std::size_t received){
				receiveHandler(socket, std::move(messageSize), error, received);
			}
		);
		if(destination != socket->remote_endpoint()) {
			sockets[destination] = socket;
		}
		sockets[socket->remote_endpoint()] = std::move(socket);
		socketConditionVariable.notify_one();
	}
	active = destination;
}

void AsioStreamingSocket::send(const ImmutableBuffer& data) {
// 	std::cout << "send " << data.size << std::endl;
	std::unique_lock<std::mutex> lock(socketMutex);
	auto& socket = *(sockets.at(active));

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

	done = false;

	while(!done) {
		std::unique_lock<std::mutex> lock(socketMutex);

		socketConditionVariable.wait(lock, [this](){
			if(closed) {
				throw(std::runtime_error("StreamingSocket: Try to receive on closed socket."));
			}
			return sockets.size() > 0;
		}); // Wait until at least one socket is connected.
		io_service.reset();
		io_service.run();

	}

	Buffer result(sizeof(SizeType));
	std::swap(result, messageBuffer);
	return result;
}

bool AsioStreamingSocket::isOpen() const {
	return acceptor.is_open();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getLocalEndpoint() const {
	return acceptor.local_endpoint();
}

AsioStreamingSocket::Endpoint AsioStreamingSocket::getRemoteEndpoint() const {
	return const_cast<const SocketMapType&>(sockets).at(active)->remote_endpoint();
}

void AsioStreamingSocket::shutdown() {
	std::unique_lock<std::mutex> lock(socketMutex);
	for(auto& socket : sockets) {
		socket.second->shutdown(Socket::shutdown_type::shutdown_both);
	}
}

void AsioStreamingSocket::close() {
	//shutdown();
	closed = true;
	if(acceptorRunning) {
		acceptorRunning = false;
		Socket s(io_service, boost::asio::ip::tcp::v4());
		//socket.open(boost::asio::ip::tcp::v4());
		s.bind(
			Endpoint(
				boost::asio::ip::address::from_string("0.0.0.0"),
				0
			)
		);
		s.connect(acceptor.local_endpoint());
		s.close();
	}

	acceptor.close();
	{
		std::unique_lock<std::mutex> lock(socketMutex);
		for(auto& socket : sockets) {
			socket.second->close();
		}
	}

// 	bool done = false;
// 	do {
// 		try {
// 			io_service.run();
// 			done = true;
// 		} catch(const std::exception&) {
//
// 		}
// 	} while(!done);
}
