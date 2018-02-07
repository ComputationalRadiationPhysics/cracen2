#include <boost/mpi.hpp>

// #include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/sockets/AsioStreaming.hpp"
#include "cracen2/sockets/BoostMpi.hpp"

#include "cracen2/util/Thread.hpp"
#include "cracen2/CracenServer.hpp"
#include "cracen2/Cracen2.hpp"

#include "cracen2/send_policies/broadcast.hpp"
#include "cracen2/send_policies/round_robin.hpp"

using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;

constexpr size_t KiB = 1024;
constexpr size_t MiB = 1024 * KiB;
constexpr size_t GiB = 1024 * MiB;

constexpr size_t frameSize = 510 * KiB;
constexpr unsigned int queueSize = 20;

constexpr auto walltime = std::chrono::seconds(120);

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
	using Frame = std::vector<std::uint8_t>;
	using MessageList = std::tuple<Frame, unsigned int>;
	using Cracen = CracenClient<Socket, MessageList>;

	static std::chrono::high_resolution_clock::time_point end;
	static bool walltimecheck() {
		static auto end = std::chrono::high_resolution_clock::now() + walltime;
		return (std::chrono::high_resolution_clock::now() < end);
	}

	Endpoint serverEp;

	TotalBandwidth(int role)
	{
//		serverEp = Endpoint(0, 1);
		serverEp = Endpoint(boost::asio::ip::address::from_string("172.24.0.17"), 5055);
// 		serverEp = Endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 5055);

		std::cout << "Connecting to " << serverEp << std::endl;

		if(role == 0) {
			std::cout << "Collector" << std::endl;
			collect();
		} else if(role == 1) {
			std::cout << "Source" << std::endl;
			source();
		} else if(role == 2) {
			std::cout << "Sink" << std::endl;
			sink();
		} else {
			std::cout << "Role must be between 0 .. 2." << std::endl;
			std::exit(1);
		}
	}

	void source() {
		Cracen cracen(serverEp, 0, Config(0).roleConnectionGraph);

    const Frame frame(frameSize);
		std::queue<std::future<void>> futures;
		for(unsigned int i = 0; i < queueSize; i++) {
			auto t = cracen.asyncSend(frame, send_policies::round_robin(1));
			for(auto& f : t) {
				futures.push(std::move(f));
			}
		}

		while(walltimecheck()) {
			if(futures.size() > queueSize) {
				futures.front().get();
				futures.pop();
			} else {
				auto t = cracen.asyncSend(frame, send_policies::round_robin(1));
				for(auto& f : t) {
					futures.push(std::move(f));
				}
			}
		}

		cracen.stop();
	}

	void sink() {
		Cracen cracen(serverEp, 1, Config(1).roleConnectionGraph);

		std::atomic<unsigned int> counter { 0 };
		auto counterThread = JoiningThread("TBW:counter", [&cracen, &counter](){

      while(walltimecheck()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				unsigned int value = counter.exchange(0);
				cracen.send(value, send_policies::round_robin(2));
			}
		});

		std::queue<std::future<Frame>> futures;
		for(unsigned int i = 0; i < queueSize; i++) {
			futures.push(cracen.template asyncReceive<Frame>());
		}

		while(walltimecheck()) {
			futures.front().get();
			futures.pop();
			counter++;
			futures.push(cracen.template asyncReceive<Frame>());
		}

		cracen.stop();
	}

	void collect() {
		Cracen cracen(serverEp, 2, Config(2).roleConnectionGraph);

		std::atomic<unsigned int> totalCounter { 0 };

		auto counterThread = JoiningThread("TBW:counterThread",[&totalCounter](){
			std::cout << "Counter started." << std::endl;
			while(walltimecheck()) {
				auto begin = std::chrono::high_resolution_clock::now();
				std::this_thread::sleep_for(std::chrono::seconds(5));
				auto end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float> time = end - begin;
				std::cout << "Last Throughput = " << totalCounter.exchange(0) * (static_cast<double>(frameSize) / GiB * 8) / time.count() << " Gbps" << std::endl;
			}
		});

		while(walltimecheck()) {
			const auto intermediate = cracen.template receive<unsigned int>();
			totalCounter += intermediate;
		}
		cracen.stop();
	}

};

int main(int argc, char* argv[]) {
	if(argc < 2) {
		std::cout << "Add role as arg. ./TotalBandwidth <0..2>" << std::endl;
		std::exit(1);
	}
	std::this_thread::sleep_for(std::chrono::seconds(1));
// 	TotalBandwidth<AsioDatagramSocket> run;
// 	TotalBandwidth<AsioStreamingSocket> run;
	TotalBandwidth<AsioStreamingSocket> run(std::atoi(argv[1]));

	return 0;
}
