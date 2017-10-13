#include "cracen2/sockets/BoostMpi.hpp"

#include "cracen2/Cracen2.hpp"
#include "cracen2/CracenServer.hpp"

#include "cracen2/send_policies/broadcast.hpp"
#include "cracen2/util/Test.hpp"


using namespace cracen2;
using namespace cracen2::util;
using namespace cracen2::sockets;

using Messages = std::tuple<int>;

struct Role {
	template <class T>
	struct InputQueueSize {
		const static size_t value = 10;
	};

	template <class T>
	using OutputQueueSize = InputQueueSize<T>;

	backend::RoleId roleId;
	std::vector<std::pair<backend::RoleId, backend::RoleId>> roleConnectionGraph;

	Role(backend::RoleId roleId) :
		roleId(roleId),
		roleConnectionGraph({ std::make_pair(0, 1) })
	{}
};

template <class T>
constexpr size_t Role::InputQueueSize<T>::value;

template <class SocketImplementation>
void cracenTest() {
	TestSuite testSuite("Cracen2 Testsuite");
	CracenServer<SocketImplementation> server;

	std::array<Cracen2<SocketImplementation, Role, Messages>, 2> cracen {{
		{ server.getEndpoint(), Role(0) },
		{ server.getEndpoint(), Role(1) }
	}};

	// Using udp, there is a chance of package loss due to collision with the older packages
	// that is why there is the wait.
	std::this_thread::sleep_for(std::chrono::milliseconds(1200));
	cracen[0].printStatus();
	cracen[1].printStatus();

	auto sendAction = util::JoiningThread([&](){
		cracen[0].send(5, send_policies::broadcast_any());
	});
	auto received = cracen[1].template receive<int>();
	std::cout << "received int = " << received << std::endl;
	testSuite.equal(received, 5, "Cracen receive test");

	cracen[0].release();
	cracen[1].release();
	server.stop();
}

int main(int, char**) {
//  	cracenTest<AsioDatagramSocket>();
//  	cracenTest<AsioStreamingSocket>();
	cracenTest<BoostMpiSocket>();
}
