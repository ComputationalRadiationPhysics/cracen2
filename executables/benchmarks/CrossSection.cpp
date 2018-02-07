// #include "cracen2/sockets/AsioDatagram.hpp"
// #include "cracen2/sockets/AsioStreaming.hpp"
// #include "cracen2/sockets/BoostMpi.hpp"
//
// #include "cracen2/Cracen2.hpp"
// #include "cracen2/CracenServer.hpp"
// #include "cracen2/send_policies/broadcast.hpp"
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
// 		roleConnectionGraph({{ std::make_pair(0, 1) }, { std::make_pair(1, 2)}})
//
// 	{}
// };
//
// struct Source {};
// struct Sink {};
//
// template <class Socket>
// struct CrossSection {
// 	static constexpr size_t frameSize = ((2 << 16) - 64);
// 	using Frame = std::array<std::uint8_t, frameSize>;
// 	using Messages = std::tuple<Frame>;
// 	using SocketImplementation = cracen2::sockets::AsioDatagramSocket;
// 	using Cracen = Cracen2<SocketImplementation, Config, Messages>;
//
// 	CrossSection(int rank) {
// 		const auto roleId = [](){
// 			if(rank == 0) return 2;
// 			return rank % 2;
// 		}
// 		Cracen client(Config(roleId));
//
// 		if(roleId == 1) {
// 			// Source
// 		}
// 		if(roleId == 2) {
// 			// Sink
// 		}
//
// 		if(roleId == 3) {
// 			// Collector
// 		}
//
// 	}
// }
//
int main(int argc, char** argv) {
// 	boost::mpi::communicator world;
// 	auto rank = world.rank();
// 	CrossSection<>(world.rank());
}
