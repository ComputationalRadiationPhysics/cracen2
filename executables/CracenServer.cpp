#include <csignal>
#include <vector>
#include <functional>

#include "cracen2/sockets/AsioStreaming.hpp"
#include "cracen2/sockets/BoostMpi.hpp"
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

int main(int, char**) {
	// Reading config

	// Adding signal handlers for clean up before termination

	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);

	// Initilise Server
	CracenServer<AsioStreamingSocket> asioStreamingServer(AsioStreamingSocket::Endpoint(boost::asio::ip::tcp::v4(), 5055));
	cleanUpActions.push_back([&asioStreamingServer](){ asioStreamingServer.stop(); });

// 	CracenServer<BoostMpiSocket> mpiserver(BoostMpiSocket::Endpoint(0,1));
// 	cleanUpActions.push_back([&mpiserver](){ mpiserver.stop(); });
// 	while(true) {};
	return 0;
}
