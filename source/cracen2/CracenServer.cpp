#include <csignal>
#include <vector>
#include <functional>

#include "cracen2/sockets/Asio.hpp"
#include "cracen2/CracenServer.hpp"

using namespace cracen2;
using namespace cracen2::sockets;

std::vector<std::function<void(void)>> cleanUpActions;

void signalHandler(int signal) {
	std::cout << "Captured signal. Exiting..." << std::endl;
	for(auto& action : cleanUpActions) {
		action();
	}
}

int main(int argc, char* argv[]) {
	// Reading config

	// Adding signal handlers for clean up before termination

	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);

	// Initilise Server

	CracenServer<AsioStreamingSocket> asioTcpServer(39001);
	cleanUpActions.push_back([&asioTcpServer](){ asioTcpServer.stop(); });

	CracenServer<AsioStreamingSocket> asioUdpServer(39002);
	cleanUpActions.push_back([&asioUdpServer](){ asioUdpServer.stop(); });

	return 0;
}
