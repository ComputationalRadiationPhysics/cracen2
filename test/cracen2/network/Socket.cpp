#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/Demangle.hpp"
#include "cracen2/network/Socket.hpp"
#include "cracen2/sockets/Asio.hpp"

#include <future>
#include <typeinfo>

using namespace cracen2::sockets;
using namespace cracen2::util;
using namespace cracen2::network;

/*
 * To add new implementations have a look at the main function
 */


template <class SocketType>
struct SocketTest {

	/**
	 * A short remark to the maximum packageCound and buffer size. The maximum queue size for udp is
	 * limited by the kernel. If the sender sends more, than the receiver consumes and the queue grows
	 * too large, the kernel will drop udp packages. This will result in a deadlock in this test,
	 * because the number of received packages must be the number of sent ones.
	 * Tcp does not have this problem, because of the flow control and blocking nature of tcp sockets.
	 *
	 * To check if frames have been dropped you can use this command:
	 * `netstat -s --udp`
	 */

	static constexpr unsigned int packageCount = 100;

	using SocketT = typename cracen2::network::Socket<SocketType>;
	using Endpoint = typename SocketT::Endpoint;

	TestSuite testSuite;

	std::promise<Endpoint> receiverEndpointPromise;

	JoiningThread sender;
	JoiningThread receiver;



	Buffer fill() {
		Buffer buffer(128);
		int i = 0;
		for(auto& b : buffer) b = i++;
		return buffer;
	}

	void send() {
		SocketT socket;
		Buffer buffer = fill();
		Endpoint destination = receiverEndpointPromise.get_future().get();
		//std::cout << "Sending to " << destination.port() << std::endl;
		socket.connect(destination);
		for(unsigned int i = 0; i < packageCount; i++) {
			try {
			socket.send(
				ImmutableBuffer(buffer.data(), buffer.size())
			);
			} catch (const std::exception& e) {
				std::cerr << "Exception thrown:" << e.what() << std::endl;
				testSuite.test(false, "Exception thrwon during send.");
				break;
			}
		}

		socket.close();
	}

	void receive() {
		SocketT socket;
		constexpr unsigned int maxPort = 40000;
		for(unsigned int port = 39001; port < maxPort; port++) {
			try {
				socket.bind(port);
				// Socket bound successfully;
				port = maxPort+1;
			} catch(const std::exception& e) {
				//std::cout << "Could not bind to port " << port << std::endl;
				//std::cout << e.what() << std::endl;
			}
		}
		//testSuite.test(socket.isOpen(), "Could not open socket");
		//std::cout << "Receiving on " << socket.getLocalEndpoint().address().to_string() << ":" << socket.getLocalEndpoint().port() << std::endl;
		receiverEndpointPromise.set_value(socket.getLocalEndpoint());
		socket.accept();
		for(unsigned int i = 0; i < packageCount; i++) {
			try {
				auto data = socket.receive();
				testSuite.equalRange(data.first, fill(), "Did not receive correct data.");
			} catch (const std::exception& e) {
				std::cerr << "Exception thrown:" << e.what() << std::endl;
				testSuite.test(false, "Exception thrwon during send.");
				break;
			}

		}

		socket.close();
	}

	SocketTest(TestSuite& parent) :
		testSuite(
			std::string("Socket test with ") + demangle(typeid(SocketType).name()) + std::string("as implementation."),
			std::cerr,
			&parent
		),
		sender(&SocketTest::send, this),
		receiver(&SocketTest::receive, this)
	{
	}
};

int main() {

	TestSuite testSuite("Socket");
	for(int i = 0; i < 100; i++) {
		SocketTest<AsioStreamingSocket>{ testSuite };
		SocketTest<AsioDatagramSocket>{ testSuite };
	}

}
