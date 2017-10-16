// #include <iostream>
// #include <vector>
// #include <iomanip>
//
// #include "cracen2/Cracen2.hpp"
// #include "cracen2/CracenServer.hpp"
// #include "cracen2/send_policies/broadcast.hpp"
// #include "cracen2/util/Signal.hpp"
//
// #include "cracen2/sockets/Asio.hpp"
//
// using namespace cracen2;
//
// struct Config {
// 	template <class T>
// 	struct InputQueueSize {
// 		const static size_t value = 10;
// 	};
//
// 	template <class T>
// 	using OutputQueueSize = InputQueueSize<T>;
//
// 	backend::RoleId roleId;
// 	std::vector<std::pair<backend::RoleId, backend::RoleId>> roleConnectionGraph;
//
// 	Config(backend::RoleId roleId) :
// 		roleId(roleId),
// 		roleConnectionGraph({ std::make_pair(0, 1) })
// 	{}
// };
// template <class T>
// constexpr size_t Config::InputQueueSize<T>::value;
// constexpr size_t Kibibyte = 1024;
// constexpr size_t Mibibyte = 1024*Kibibyte;
//
// std::vector<std::string> split(const std::string &s, char delim) {
// 	std::stringstream ss(s);
// 	std::vector<std::string> result;
// 	std::string line;
// 	while(std::getline(ss, line, delim)) {
// 		result.push_back(line);
// 	}
//     return result;
// }
//
// struct End {};
//
int main() {
//
// 	using Frame = std::vector<std::uint8_t>;
// 	using Messages = std::tuple<Frame, End>;
// 	using SocketImplementation = cracen2::sockets::AsioDatagramSocket;
// 	using Cracen = Cracen2<SocketImplementation, Config, Messages>;
//
// 	CracenServer<SocketImplementation> server;
// 	auto serverEndpoint = server.getEndpoint();
//
// 	util::JoiningThread source([serverEndpoint](){
// 		constexpr auto roleId = 0;
// 		Cracen cracen(serverEndpoint, Config(roleId));
// 		{
// 			auto view = cracen.getRoleCommunicatorMapReadOnlyView(
// 				[roleId](const Cracen::RoleCommunicatorMap& roleComMap) -> bool {
// 					const auto neighborId = 1 - roleId;
// 					if(roleComMap.count(neighborId) == 1) {
// 						return roleComMap.at(neighborId).size() > 0;
// 					} else {
// 						return false;
// 					}
// 				}
// 			);
// 		} 	// Wait for the first participant on roleId == 0 to connect
//
// 		// Sending data
// 		std::vector<std::size_t> sizes {
// 			1,
// 			8,
// 			32,
// 			64,
// 			128,
// 			256,
// 			512,
// 			1*Kibibyte,
// 			2*Kibibyte,
// 			4*Kibibyte,
// 			8*Kibibyte,
// 			16*Kibibyte,
// 			32*Kibibyte,
// 			64*Kibibyte - 64 // Little under 64K for Header
// 		};
// 		Frame frame;
// 		for(auto size : sizes) {
// 			frame.resize(size);
// 			auto start = std::chrono::high_resolution_clock::now();
// 			auto end = start + std::chrono::seconds(60);
// 			while(std::chrono::high_resolution_clock::now() < end) {
// 				cracen.send(frame, send_policies::broadcast_any());
// 			}
// 		}
// 		auto end = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(200);
// 		while(std::chrono::high_resolution_clock::now() < end) {
// 			cracen.send(End(), send_policies::broadcast_any());
// 		}
// 	});
//
// 	util::JoiningThread sink([serverEndpoint](){
// 		constexpr auto roleId = 1;
// 		Cracen cracen(serverEndpoint, Config(roleId));
// 		{
// 			auto view = cracen.getRoleCommunicatorMapReadOnlyView(
// 				[roleId](const Cracen::RoleCommunicatorMap& roleComMap) -> bool {
// 					const auto neighborId = 1 - roleId;
// 					if(roleComMap.count(neighborId) == 1) {
// 						return roleComMap.at(neighborId).size() > 0;
// 					} else {
// 						return false;
// 					}
// 				}
// 			);
// 		} 	// Wait for the first participant on roleId == 0 to connect
//
// 		// Receiving Data
//
// 		bool running = true;
//
// 		std::atomic<unsigned int> frameCounter(0);
// 		std::atomic<unsigned int> frameSize(0);
//
// 		util::JoiningThread outputThread([&frameCounter, &frameSize, &running](){
// 			std::map<std::size_t, std::vector<double>> dataRatesInMib;
// 			double rateInMiBs = 0;
// 			do {
// 				const auto start = std::chrono::high_resolution_clock::now();
// 				unsigned int frameCounterOld = frameCounter;
//
// 				std::this_thread::sleep_for(std::chrono::seconds(1));
// 				unsigned int frameCounterNew = frameCounter;
// 				const auto end = std::chrono::high_resolution_clock::now();
//
//
// 				const auto frames =  frameCounterNew - frameCounterOld;
// 				const auto transferedMib =  static_cast<double>(frames * frameSize) / Mibibyte;
// 				const auto elapsedTimeinS = std::chrono::duration<double>(end - start).count();
// 				rateInMiBs = transferedMib / elapsedTimeinS;
//
// 				dataRatesInMib[frameSize].push_back(rateInMiBs);
// 				std::cout << "Framesize = " << frameSize << " Byte: " << rateInMiBs << " MiB/s" << std::endl;
// 			} while(rateInMiBs > 0.001);
// 			running = false;
// 			bool done = false;
// 			unsigned int i = 0;
// 			std::cout << std::setfill (' ') <<  std::setprecision(1) << std::fixed << std::setw (7);
// 			for(auto ratePair : dataRatesInMib) {
// 				std::cout << ratePair.first << "	";
// 			}
// 			std::cout << std::endl;
// 			do {
// 				done = true;
// 				for(auto ratePair : dataRatesInMib) {
// 					if(ratePair.second.size() > i) {
// 						std::cout << ratePair.second[i];
// 						done = false;
// 					} else {
// 						std::cout << std::string(7, ' ');
// 					}
// 					std::cout << "	";
// 				}
// 				std::cout << std::endl;
// 				i++;
// 			} while(!done);
// 		});
//
// 		while(running) {
// 			Cracen::Visitor visitor(
// 				[&frameSize, &frameCounter](const Frame frame){
// 					frameSize = frame.size();
// 					frameCounter++;
// 				},
// 				[&running](const End) {
// 					running = false;
// 				}
// 			);
// 			cracen.receive(visitor);
// 		}
//
// 	});
//
	return 0;
}
