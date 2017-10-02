#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/network/Communicator.hpp"

#include "cracen2/sockets/Asio.hpp"
#include "cracen2/sockets/BoostMpi.hpp"

#include <future>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;

constexpr unsigned long Kilobyte = 1024;
constexpr unsigned long Megabyte = 1024*Kilobyte;
constexpr unsigned long Gigabyte = 1024*Megabyte;


// constexpr size_t bigMessageSize = 64*Kilobyte;
// constexpr size_t bigMessageSize = std::numeric_limits<std::uint16_t>::max()- 128;

const std::vector<size_t> frameSize {
	//1*Kilobyte,
// 	//16*Kilobyte,
// 	64*Kilobyte - 128,
// 	256*Kilobyte,
// 	512*Kilobyte,
	1*Megabyte,
	2*Megabyte
};

constexpr unsigned long volume = 1*Gigabyte;

template <class SocketImplementation>
struct CommunicatorTest {
	using TagList = std::tuple<int, char, std::string, std::vector<std::uint8_t>>;
	using CommunicatorType = typename cracen2::network::Communicator<SocketImplementation, TagList>;
	using Endpoint = typename CommunicatorType::Endpoint;

	TestSuite& testSuite;

	std::promise<Endpoint> server;
	std::promise<void> clientShutdown;

	JoiningThread sourceThread;
// 	JoiningThread sinkThread;

	void source() {
		CommunicatorType communicator;
		const Endpoint ep = server.get_future().get();
		communicator.sendTo(5, ep);
		communicator.sendTo('c', ep);
		communicator.sendTo(std::string("Hello World!"), ep);
		communicator.sendTo(std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }}, ep);
		communicator.sendTo(5, ep);
		communicator.sendTo('c', ep);
		communicator.sendTo(std::string("Hello World!"), ep);
		communicator.sendTo(std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }}, ep);
		auto msg = std::vector<std::uint8_t>(32*Kilobyte);
		communicator.sendTo(msg, ep);
		communicator.sendTo(5, ep);

		testSuite.equal(communicator.template receive<std::string>(), std::string("answer"), "Answer Test");
		clientShutdown.get_future().wait();
	};

	void sink() {

		CommunicatorType communicator;
		communicator.bind();
		server.set_value(communicator.getLocalEndpoint());

		testSuite.test(communicator.isOpen(), "Socket is not open.");

		auto visitor = CommunicatorType::make_visitor(
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

		auto res = communicator.template receiveFrom<int>();
		const Endpoint ep = res.second;
		testSuite.equal(res.first, 5, "receive<int> test");
		testSuite.equal(communicator.template receive<char>(), 'c', "receive<char> test");
		testSuite.equal(communicator.template receive<std::string>(), std::string("Hello World!"), "receive<std::string> test");
		testSuite.equalRange(
			communicator.template receive<std::vector<std::uint8_t>>(),
			std::vector<std::uint8_t>{{ 1, 2, 3, 4, 5, 6 }},
			"receive<std::string> test"
		);
		testSuite.equalRange(
			communicator.template receive<std::vector<std::uint8_t>>(),
			std::vector<std::uint8_t>(32*Kilobyte),
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

		communicator.sendTo(std::string("answer"), ep);
		clientShutdown.set_value();
	};

	CommunicatorTest(TestSuite& testSuite) :
 		testSuite(testSuite),
 		sourceThread(&CommunicatorTest::source, this)
 		//sinkThread(&CommunicatorTest::sink, this)
	{
		sink();
	}

};

template <class SocketImplementation>
struct BandwidthTest {

	using Chunk = std::vector<std::uint8_t>;
	using TagList = std::tuple<Chunk>;
	using CommunicatorType = Communicator<SocketImplementation, TagList>;
	using Endpoint = typename CommunicatorType::Endpoint;
	BandwidthTest() {


		CommunicatorType alice;
		alice.bind();
		auto aliceEp = alice.getLocalEndpoint();

		CommunicatorType bob;

		JoiningThread bobThread([&bob, &aliceEp](){
			Chunk chunk;
			for(auto size : frameSize) {
				if(std::is_same<SocketImplementation, AsioDatagramSocket>::value && size > 64*Kilobyte) {
					break;
				}
				chunk.resize(size);

				std::vector<std::future<void>> requests;

				for(unsigned long sent = 0; sent < volume;sent+=size) {
					requests.push_back(bob.asyncSendTo(chunk, aliceEp));
				}

				for(auto& r : requests) {
					r.get();
				}

			}
		});


		for(auto size : frameSize) {
			if(std::is_same<SocketImplementation, AsioDatagramSocket>::value && size > 64*Kilobyte) {
				break;
			}
			std::vector<std::future<Chunk>> requests;
			requests.reserve(volume / size + 1);
			unsigned long received;
			auto begin = std::chrono::high_resolution_clock::now();
			{
				for(received = 0; received < volume;received+=size) {
					requests.push_back(alice.template asyncReceive<Chunk>());
				}
				for(auto& r : requests) {
					try {
						r.get();
					} catch(const std::exception& e) {
						std::cout << "alice catched exception: " << e.what() << std::endl;
					}
				}
				alice.close();
			}
			auto end = std::chrono::high_resolution_clock::now();

			std::cout
				<< "Bandwith of Communicator<" << demangle(typeid(SocketImplementation).name()) << ", ...> = "
				<< (static_cast<double>(received) * 1000 / Gigabyte / std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() * 8)
				<< " Gbps for " << size << " Byte Frames"
				<< std::endl;
		}
	}

}; // end of class BandwidthTest

int main() {

	TestSuite testSuite("Communicator");

// 	{	CommunicatorTest<AsioDatagramSocket> datagramCommunicator(testSuite); }
// 	{	CommunicatorTest<AsioStreamingSocket> streamingCommunicator(testSuite); }
 	{	CommunicatorTest<BoostMpiSocket> mpiCommunicator(testSuite); }
// 	{ BandwidthTest<AsioDatagramSocket> udpTest; }
// 	{ BandwidthTest<AsioStreamingSocket> tcpTest; }
// 	{ BandwidthTest<BoostMpiSocket> mpiTest; }

}
