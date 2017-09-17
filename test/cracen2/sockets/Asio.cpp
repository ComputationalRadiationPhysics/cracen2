#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/Asio.hpp"
#include "cracen2/util/Demangle.hpp"
#include <future>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;

using UdpSocket = AsioSocket<AsioProtocol::udp>;
using TcpSocket = AsioSocket<AsioProtocol::tcp>;

std::promise<UdpSocket::Endpoint> udpEndpoint;
std::promise<TcpSocket::Endpoint> tcpEndpoint;

template <class Socket>
struct SocketTest {
	using Endpoint = typename Socket::Endpoint;
	using Acceptor = typename Socket::Acceptor;

	static constexpr const char* message = "Hello World!";
	SocketTest(TestSuite& testSuite) {

		Endpoint endpoint;

		Acceptor acceptor;
		try {
			acceptor.bind();
		} catch(const std::exception& e) {
			std::cout << e.what() << std::endl;
		}

		const Endpoint sinkEndpoint = acceptor.getLocalEndpoint();
		JoiningThread source([sinkEndpoint](){
			Socket socket;
			socket.connect(sinkEndpoint);

			std::string s(message);
			ImmutableBuffer buffer(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
			socket.send(buffer);
		});

		// Sink part
		auto socket = acceptor.accept();
		const auto buffer = socket.receive();
		std::string s(reinterpret_cast<const char*>(buffer.data()), buffer.size());
		testSuite.equal(s, std::string(message), "Send/Receive test for " + getTypeName<Socket>());
	}
};

int main() {

	TestSuite testSuite("Asio");

	{ SocketTest<AsioStreamingSocket> test(testSuite); }
 	{ SocketTest<AsioDatagramSocket> test(testSuite); }
}
