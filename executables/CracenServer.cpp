#include <csignal>
#include <vector>
#include <functional>

#include "cracen2/sockets/Asio.hpp"
#include "cracen2/CracenServer.hpp"

using namespace cracen2;
using namespace cracen2::sockets;

std::vector<std::function<void(void)>> cleanUpActions;

void signalHandler(int) {
	std::cout << "Captured signal. Exiting..." << std::endl;
	for(auto& action : cleanUpActions) {
		action();
	}
	exit(0);
}

int main(int argc, char* argv[]) {
	// Reading config


	// auto port1 = (argc > 2) ? std::atoi(argv[1]) : 39391;
	auto port2 = (argc > 3) ? std::atoi(argv[2]) : 39392;

	// Adding signal handlers for clean up before termination

	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);

	// Initilise Server
	// CracenServer<AsioStreamingSocket> asioTcpServer(port1);
	// cleanUpActions.push_back([&asioTcpServer](){ asioTcpServer.stop(); });

	CracenServer<AsioDatagramSocket> asioUdpServer(
		typename AsioDatagramSocket::Endpoint(
			boost::asio::ip::address::from_string("0.0.0.0"),
			port2
		)
	);
	cleanUpActions.push_back([&asioUdpServer](){ asioUdpServer.stop(); });

	return 0;
}
