#include <boost/mpi.hpp>

#include "cracen2/sockets/Asio.hpp"
#include "cracen2/sockets/BoostMpi.hpp"

std::ostream& operator<<(std::ostream& lhs, const cracen2::sockets::BoostMpiSocket::Endpoint& rhs) {
	lhs << "{ " << rhs.first << ", " << rhs.second << " }";
	return lhs;
}

#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/Cracen2.hpp"

#include "cracen2/send_policies/broadcast.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;

constexpr size_t KiB = 1024;
constexpr size_t MiB = 1024 * KiB;

constexpr size_t frameSize = 16 * KiB;


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
		roleConnectionGraph({
			std::make_pair(0, 1),
			std::make_pair(1, 2)
		})
	{}
};

template <class T>
constexpr size_t Config::InputQueueSize<T>::value;

template <class Socket>
struct TotalBandwidth {

	using Endpoint = typename Socket::Endpoint;
	using Frame = std::array<std::uint8_t, frameSize>;
	using MessageList = std::tuple<Frame, unsigned int>;
	using Cracen = Cracen2<Socket, Config, MessageList>;

	Endpoint serverEp;


	TotalBandwidth()
	{
		boost::mpi::environment env;
		boost::mpi::communicator world;

		if(world.size() < 4) {
			std::cout << "There must be at least 4 processes. Try {mpiexec} -n 4 ./TotalBandwidth" << std::endl;
			exit(1);
		}
		const auto rank = world.rank();
		// Map from rank to action:

// 		std::uint16_t port;
// 		std::string address;
		if(rank == 0) {
			CracenServer<Socket> server;
			serverEp = server.getEndpoint();
// 			port = serverEp.port();
// 			address = serverEp.address().to_string();
// 			boost::mpi::broadcast(world, port, 0);
// 			boost::mpi::broadcast(world, address, 0);
			boost::mpi::broadcast(world, serverEp, 0);

		} // Server will stay here

		std::cout << "rank " << rank << " start broadcast." << std::endl;

// 		boost::mpi::broadcast(world, port, 0);
// 		boost::mpi::broadcast(world, address, 0);
		boost::mpi::broadcast(world, serverEp, 0);

// 		serverEp = Endpoint(boost::asio::ip::address::from_string(address), port);
		std::cout << "Connecting to " << serverEp << std::endl;

		if(rank == 1) {
			collect();
		} else if(rank % 2 == 0) {
			source();
		} else {
			sink();
		}
	}

	void server() {

	}

	void source() {
		Cracen cracen(serverEp, Config(0));
		Frame frame {};
		while(true) {
			cracen.send(frame, send_policies::broadcast_role(1));
		}
	}

	void sink() {
		Cracen cracen(serverEp, Config(1));

		std::atomic<unsigned int> counter;
		auto counterThread = JoiningThread([&cracen, &counter](){
			while(true) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				unsigned int value = counter.exchange(0);
				cracen.send(value, send_policies::broadcast_role(2));
			}
		});

		while(true) {
			cracen.template receive<Frame>();
			counter++;
		}

	}

	void collect() {
		Cracen cracen(serverEp, Config(2));

		std::atomic<unsigned int> totalCounter;

		auto counterThread = JoiningThread([&totalCounter](){
			while(true) {
				auto begin = std::chrono::high_resolution_clock::now();
				std::this_thread::sleep_for(std::chrono::seconds(5));
				auto end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float> time = end - begin;
				std::cout << "Last Throughput = " << totalCounter.exchange(0) * (static_cast<double>(frameSize) / MiB) / time.count() << " MiB/s" << std::endl;
			}
		});

		while(true) {
			const auto intermediate = cracen.template receive<unsigned int>();
			totalCounter += intermediate;
		}

	}

};

int main(int argc, char* argv[]) {

// 	TotalBandwidth<AsioDatagramSocket> run;
// 	TotalBandwidth<AsioStreamingSocket> run;
	TotalBandwidth<BoostMpiSocket> run;


	return 0;
}
