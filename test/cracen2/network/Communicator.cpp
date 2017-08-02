#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/Asio.hpp"
#include "cracen2/network/Communicator.hpp"

#include <future>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;

template <class SocketImplementation>
struct CommunicatorTest {
	using TagList = std::tuple<int, char, std::string>;
	using Communicator = typename cracen2::network::Communicator<SocketImplementation, TagList>;
	using Visitor = typename Communicator::Visitor;
	using Endpoint = typename Communicator::Endpoint;
	using Port = typename Communicator::Port;

	TestSuite& testSuite;

	std::promise<Endpoint> server;
	std::promise<void> clientShutdown;

	JoiningThread sourceThread;
	JoiningThread sinkThread;

	void source() {
		Communicator communicator;
		const Endpoint ep = server.get_future().get();
		communicator.connect(ep);
		communicator.send( 5);
		communicator.send('c');
		communicator.send(std::string("Hello World!"));
		communicator.send(5);
		communicator.send('c');
		communicator.send(std::string("Hello World!"));
		communicator.send(5);

		testSuite.equal(communicator.template receive<std::string>(), std::string("answer"), "Answer Test");
		clientShutdown.get_future().wait();
		communicator.close();
	};

	void sink() {

		Communicator communicator;
		constexpr Port portMin = 39000;
		constexpr Port portMax = 40000;

		for(Port p = portMin; p < portMax; p++) {
			try {
				communicator.bind(p);
				p = portMax + 1;
			} catch(const std::exception&) {
				// Could not bind socket to port
			}
		}

		testSuite.test(communicator.isOpen(), "Socket is not open.");

		server.set_value(communicator.getLocalEndpoint());
		communicator.accept();

		Visitor visitor(
			[&](int value) { testSuite.equal(value, 5, "Visitor test for int"); },
			[&](char value) { testSuite.equal(value, 'c', "Visitor test for char"); },
			[&](std::string value) { testSuite.equal(value, std::string("Hello World!"), "Visitor test for std::string"); }
		);

		for(int i = 0; i < 3; i++) {
			communicator.receive(visitor);
		}

		testSuite.equal(communicator.template receive<int>(), 5, "receive<int> test");
		testSuite.equal(communicator.template receive<char>(), 'c', "receive<char> test");
		testSuite.equal(communicator.template receive<std::string>(), std::string("Hello World!"), "receive<std::string> test");

		bool receiveFailed = false;
		try {
			std::string result = communicator.template receive<std::string>();
			testSuite.test(false, std::string("Received a std::string (\"") + result + "\"), but should have received an int");
		} catch(const std::exception&) {
			receiveFailed = true;
		}
		testSuite.test(receiveFailed, "Typed receive with wrong type test for thrown exception");

		communicator.send(std::string("answer"));
		clientShutdown.set_value();
		communicator.close();
	};

	CommunicatorTest(TestSuite& testSuite) :
		testSuite(testSuite),
		sourceThread(&CommunicatorTest::source, this),
		sinkThread(&CommunicatorTest::sink, this)
	{
	}
};

int main() {
	TestSuite testSuite("Communicator");

	CommunicatorTest<AsioStreamingSocket> streamingCommunicator(testSuite);
	CommunicatorTest<AsioStreamingSocket> datagramCommunicator(testSuite);

}
