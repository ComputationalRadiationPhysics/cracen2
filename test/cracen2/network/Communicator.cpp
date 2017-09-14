#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/Asio.hpp"
#include "cracen2/network/Communicator.hpp"

#include <future>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;

constexpr size_t bigMessageSize = 48*1024;

template <class SocketImplementation>
struct CommunicatorTest {
	using TagList = std::tuple<int, char, std::string, std::vector<std::uint8_t>>;
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
		communicator.send(std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }});
		communicator.send(5);
		communicator.send('c');
		communicator.send(std::string("Hello World!"));
		communicator.send(std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }});
		auto msg = std::vector<std::uint8_t>(bigMessageSize);
		communicator.send(msg);
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
			[&](std::string value) { testSuite.equal(value, std::string("Hello World!"), "Visitor test for std::string"); },
			[&](std::vector<std::uint8_t> value) {
				testSuite.equalRange(
					value,
					std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }},
					"receive<std::string> test"
				);
			}
		);

		for(int i = 0; i < 4; i++) {
			communicator.receive(visitor);
		}

		testSuite.equal(communicator.template receive<int>(), 5, "receive<int> test");
		testSuite.equal(communicator.template receive<char>(), 'c', "receive<char> test");
		testSuite.equal(communicator.template receive<std::string>(), std::string("Hello World!"), "receive<std::string> test");
		testSuite.equalRange(
			communicator.template receive<std::vector<std::uint8_t>>(),
			std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }},
			"receive<std::string> test"
		);
		testSuite.equalRange(
			communicator.template receive<std::vector<std::uint8_t>>(),
			std::vector<std::uint8_t>(bigMessageSize),
			"receive<std::string> test"
		);
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

template <class SocketImplementation>
struct BandwidthTest {

	using Chunk = std::array<std::uint8_t, bigMessageSize>;
	using TagList = std::tuple<Chunk>;

	BandwidthTest() {

		Communicator<SocketImplementation, TagList> alice;
		for(unsigned short port = 39000; port < 40000; port++) {
			try{
				alice.bind(port);
				break;
			} catch(const std::exception&) {

			}
		}

		Communicator<SocketImplementation, TagList> bob;
		bob.connect(alice.getLocalEndpoint());
		alice.accept();


		constexpr unsigned int Kilobyte = 1024;
		constexpr unsigned int Megabyte = 1024*Kilobyte;
		constexpr unsigned int Gigabyte = 1024*Megabyte;
		constexpr unsigned int volume = 1*Gigabyte;
		unsigned int sent = 0;

		unsigned int received = 0;
		auto begin = std::chrono::high_resolution_clock::now();
		{
			DetatchingThread bobThread([&bob, &sent, &received](){
				Chunk chunk;
				while(received * bigMessageSize < volume) {
					sent += bigMessageSize;
					try {
						bob.send(chunk);
					} catch(const std::exception&) {
					}
				}
			});

			JoiningThread aliceThread([&alice, &received](){
				for(received = 0; received * bigMessageSize < volume;received++) {
					alice.template receive<Chunk>();
				}
			});
		}

		auto end = std::chrono::high_resolution_clock::now();
		std::cout
			<< "Bandwith of Communicator<" << demangle(typeid(SocketImplementation).name()) << ", ...> = "
			<< (static_cast<double>(sent) * 1000 / Gigabyte / std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() )
			<< " Gb/s"
			<< std::endl;
	}

}; // end of class BandwidthTest

int main() {
	TestSuite testSuite("Communicator");

	CommunicatorTest<AsioStreamingSocket> streamingCommunicator(testSuite);
	CommunicatorTest<AsioDatagramSocket> datagramCommunicator(testSuite);

	BandwidthTest<AsioStreamingSocket>();
	BandwidthTest<AsioDatagramSocket>();

}
