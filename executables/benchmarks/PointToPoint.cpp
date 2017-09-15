#include <iostream>
#include <vector>

#include "cracen2/Cracen2.hpp"
#include "cracen2/send_policies/broadcast.hpp"
#include "cracen2/util/Signal.hpp"

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

	Config(backend::RoleId roleId) :
		roleId(roleId),
		roleConnectionGraph({ std::make_pair(0, 1) })
	{}
};
template <class T>
constexpr size_t Config::InputQueueSize<T>::value;
constexpr size_t Kibibyte = 1024;
constexpr size_t Mibibyte = 1024*Kibibyte;
constexpr size_t Gibibyte = 1024*Mibibyte;


std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::vector<std::string> result;
	std::string line;
	while(std::getline(ss, line, delim)) {
		result.push_back(line);
	}
    return result;
}

struct End {};

int main(int argc, const char* argv[]) {
	if(argc <= 2) {
		std::cerr << "No server supplied. Call 'PointToPointBenchmark <x>.<x>.<x>.<x>:<port> <role{0-1}>" << std::endl;
	}

	auto endpointStrings = split(argv[1], ':');
	using SocketImplementation = sockets::AsioDatagramSocket;
	auto serverEndpoint = SocketImplementation::Endpoint(
		boost::asio::ip::address_v4::from_string(endpointStrings[0]),
		std::atoi(endpointStrings[1].c_str())
	);

	backend::RoleId roleId = std::atoi(argv[2]);

	using Frame = std::vector<int>;
	using Messages = std::tuple<Frame, End>;
	using Cracen = Cracen2<SocketImplementation, Config, Messages>;
	Cracen cracen(serverEndpoint, Config(roleId));
	util::SignalHandler::handleAll(
		[&cracen](int sig) {
			std::cout << "Catched signal " << sig << std::endl;
			std::cout << "Release the cracen." << std::endl;
			cracen.release();
			exit(0);
		}
	);

	std::cout << "Connected" << std::endl;

	// Wait for the first participant on roleId == 0 to connect
	{
		auto view = cracen.getRoleCommunicatorMapReadOnlyView(
			[roleId](const Cracen::RoleCommunicatorMap& roleComMap) -> bool {
				const auto neighborId = 1 - roleId;
				if(roleComMap.count(neighborId) == 1) {
					return roleComMap.at(neighborId).size() > 0;
				} else {
					return false;
				}
			}
		);
	} // The brackets are for the deletion of the view. If it persist till the end of the runtime, no changes to the map can be made!

	cracen.printStatus();

	Frame frame;
	if(roleId == 0) {
		for(auto size = 1*Kibibyte; size < 64*Kibibyte; size += Kibibyte) {
			frame.resize(size);
			auto start = std::chrono::high_resolution_clock::now();
			auto end = start + std::chrono::seconds(30);
			while(std::chrono::high_resolution_clock::now() < end) {
				cracen.send(frame, send_policies::broadcast_any());
			}
		}
		cracen.send(End(), send_policies::broadcast_any());
	} else {
		bool running = true;

		std::atomic<unsigned int> frameCounter(0);
		std::atomic<unsigned int> frameSize(0);

		util::JoiningThread outputThread([&frameCounter, &frameSize, &running](){
			while(running) {
				const auto start = std::chrono::high_resolution_clock::now();
				unsigned int frameCounterOld = frameCounter;

				std::this_thread::sleep_for(std::chrono::seconds(4));
				unsigned int frameCounterNew = frameCounter;
				const auto end = std::chrono::high_resolution_clock::now();


				const auto frames =  frameCounterNew - frameCounterOld;
				const auto transferedMib =  static_cast<double>(frames * frameSize) / Mibibyte;
				const auto elapsedTimeinS = std::chrono::duration<double>(end - start).count();
				const auto rateInMiBs = transferedMib / elapsedTimeinS;

				std::cout << "Framesize = " << (frameSize / Kibibyte) << " KiB: " << rateInMiBs << " MiB/s" << std::endl;
			}
		});

		while(running) {
			Cracen::Visitor visitor(
				[&frameSize, &frameCounter](const Frame frame){
					frameSize = frame.size();
					frameCounter++;
				},
				[&running](End){
					running = false;
				}
			);
			cracen.receive(visitor);
		}
	}

	return 0;
}
