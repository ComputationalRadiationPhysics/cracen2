#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/sockets/Asio.hpp"
#include <future>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;

using UdpSocket = AsioSocket<AsioProtocol::udp>;
using TcpSocket = AsioSocket<AsioProtocol::tcp>;

std::promise<UdpSocket::Endpoint> udpEndpoint;
std::promise<TcpSocket::Endpoint> tcpEndpoint;

void udpTest(TestSuite& testSuite) {
	using Endpoint = UdpSocket::Endpoint;

	UdpSocket socket;
	for(unsigned int port = 5000; port < 6000; port++) {
		try {
			socket.bind(port);
			break;
		} catch(const std::exception& e) {

		}
	}
	udpEndpoint.set_value(socket.getLocalEndpoint());
	Buffer data(128);
	for(unsigned int i = 0; i < data.size(); i++) {
		data[i] = i;
	}

	JoiningThread senderThread([data](){
		UdpSocket socket;
		Endpoint destination(udpEndpoint.get_future().get());

		ImmutableBuffer buffer(data.data(), data.size());
		socket.sendTo(destination, buffer);
		socket.close();
	});

	Endpoint sender;
	auto result = socket.receiveFrom();
	socket.close();

	testSuite.equalRange(result.first, data, "udp socket");
}

void tcpTest(TestSuite& testSuite) {

	Buffer data(128);
	for(unsigned int i = 0; i < data.size(); i++) {
		data[i] = i;
	}

	TcpSocket socket;
	for(unsigned int port = 5000; port < 6000; port++) {
		try {
			socket.bind(port);
			break;
		} catch(const std::exception& e) {

		}
	}
	tcpEndpoint.set_value(socket.getLocalEndpoint());

	JoiningThread senderThread([data](){

		TcpSocket socket;
		socket.connect(tcpEndpoint.get_future().get());

		ImmutableBuffer buffer(data.data(), data.size());
		socket.send(buffer);
		socket.close();
	});

	socket.accept();
	auto result = socket.receive(data.size());
	socket.close();

	testSuite.equalRange(result, data, "udp socket");
}

int main() {

	TestSuite testSuite("Asio");

//	udpTest(testSuite);
	tcpTest(testSuite);
}
