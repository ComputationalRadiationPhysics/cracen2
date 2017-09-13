#include <iostream>

#include "cracen2/Cracen2.hpp"
#include "cracen2/send_policies/broadcast.hpp"

#include "cracen2/sockets/Asio.hpp"

using namespace cracen2;

struct Config {
	template <class T>
	struct InputQueueSize {
		const static size_t value = 10;
	};

	template <class T>
	using OutputQueueSize = InputQueueSize<T>;

	backend::RoleId roleId;
	std::vector<std::pair<backend::RoleId, backend::RoleId>> roleConnectionGraph;

	Config() :
		roleId(0),
		roleConnectionGraph({ std::make_pair(0, 0) })
	{}
};
template <class T>
constexpr size_t Config::InputQueueSize<T>::value;

constexpr size_t Kilobyte = 1024;
using Messages = std::tuple<
	int,
	std::array<std::uint8_t, 32*Kilobyte>
>;

std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::vector<std::string> result;
	std::string line;
	while(std::getline(ss, line, delim)) {
		result.push_back(line);
	}
    return result;
}

int main(int argc, const char* argv[]) {
	if(argc <= 1) {
		std::cerr << "No server supplied. Call 'PointToPointBenchmark <x>.<x>.<x>.<x>:<port>" << std::endl;
	}

	auto endpointStrings = split(argv[1], ':');
	using SocketImplementation = sockets::AsioDatagramSocket;
	auto serverEndpoint = SocketImplementation::Endpoint(
		boost::asio::ip::address_v4::from_string(endpointStrings[0]),
		std::atoi(endpointStrings[1].c_str())
	);

	Cracen2<SocketImplementation, Config, Messages> cracen(serverEndpoint, Config());
	std::cout << "Connected" << std::endl;

	bool ready = false;
	// Wait for the first participant on roleId == 0 to connect
	do {
		auto view = cracen.getRoleCommunicatorMapReadOnlyView();
		const auto& roleComMap = view->get();
		ready = roleComMap.size() > 0;
	} while(!ready);

	std::cout << "Sending 5" << std::endl;
	cracen.send(5, send_policies::broadcast_any());
	std::cout << "Receiving..." << std::endl;
	std::cout << "received: " << cracen.receive<int>() << std::endl;

	std::cout << "Release" << std::endl;
	cracen.release();
	return 0;
}
