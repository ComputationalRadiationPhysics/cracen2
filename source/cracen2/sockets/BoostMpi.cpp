#include "cracen2/sockets/BoostMpi.hpp"
#include "cracen2/util/ThreadPool.hpp"

#include <cstdlib>
#include <ctime>
#include <limits>
#include <cstring>
#include <cstdint>
// #include <boost/serialization/vector.hpp>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;
using namespace cracen2::sockets::detail;

using Datagram = BoostMpiSocket::Datagram;
using Endpoint = BoostMpiSocket::Endpoint;

std::map<
	BoostMpiSocket::Endpoint,
	std::queue<
		std::promise<Datagram>
	>
> pendingProbes;

struct PendingReceive {
	boost::mpi::request headerRequest;
	boost::mpi::request bodyRequest;
	std::promise<Datagram> promise;
	std::unique_ptr<std::uint8_t[]> headerBuffer;
	std::unique_ptr<std::uint8_t[]> bodyBuffer;
};

std::queue<PendingReceive> pendingReceives;

std::queue<
	std::tuple<
		boost::mpi::request,
		boost::mpi::request,
		std::promise<void>,
		std::shared_ptr<std::uint8_t>
	>
> pendingSends;

bool pendingProbeTrackerRunning = false;
bool pendingReceiveTrackerRunning = false;
bool pendingSendTrackerRunning = false;

std::mutex EndpointFactory::mutex;
std::set<BoostMpiSocket::Endpoint> EndpointFactory::blockedEndpoints;

std::unique_ptr<boost::mpi::environment> env;
std::unique_ptr<boost::mpi::communicator> world;

boost::asio::io_service io_service;

JoiningThread mpiThread([](){
	std::srand(std::time(0));

	env = std::make_unique<boost::mpi::environment>(boost::mpi::threading::funneled);
	world = std::make_unique<boost::mpi::communicator>();

	io_service.run();
});

boost::asio::io_service::work work(io_service);



BoostMpiSocket::BoostMpiSocket() {
	// Thread function must be set after io_service::work object
	auto rankPromise = std::make_shared<std::promise<int>>();
	auto rankFuture = rankPromise->get_future();
	io_service.post([rankPromise = std::move(rankPromise)]() {
		rankPromise->set_value(world->rank());
	});

	endpointFactory.rank = rankFuture.get();
};

BoostMpiSocket::~BoostMpiSocket() {
	close();
}


void trackAsyncSend() {
	while(pendingSends.size() > 0) {
		auto& p = pendingSends.front();
		try {
			auto statusHeader = std::get<0>(p).test();
			auto statusBody = std::get<1>(p).test();
			if(statusHeader && statusBody) {
				// Send completed
				std::get<std::promise<void>>(p).set_value();
				pendingSends.pop();
			} else {
				break;
			}
		} catch(...) {
			std::get<std::promise<void>>(p).set_exception(std::current_exception());
			pendingSends.pop();
		}
	}

	if(pendingSends.size() == 0) {
		pendingSendTrackerRunning = false;
	} else {
		io_service.post(&trackAsyncSend);
	}
}

void trackAsyncReceive() {

	while(pendingReceives.size() > 0) {
		auto& pendingReceive = pendingReceives.front();
		try {
			auto headerStatus = pendingReceive.headerRequest.test();
			auto bodyStatus = pendingReceive.bodyRequest.test();

			if(headerStatus && bodyStatus) {

				Endpoint remote;
				remote.first = headerStatus->source();
				remote.second = *reinterpret_cast<decltype(remote.second)*>(pendingReceive.headerBuffer.get());


				const auto customHeaderSize = headerStatus->count<std::uint8_t>().get() - sizeof(remote.second);
				Buffer customHeader(customHeaderSize);
				std::memcpy(customHeader.data(), pendingReceive.headerBuffer.get() + sizeof(remote.second) , customHeaderSize);

				auto size = bodyStatus->count<std::uint8_t>().get();
				pendingReceive.promise.set_value(
					Datagram {
						std::move(customHeader),
						Buffer(
							std::move(pendingReceive.bodyBuffer),
							size
						),
						remote
					}
				);
				pendingReceives.pop();
			} else {
				break;
			}
		} catch(...) {
			pendingReceive.promise.set_exception(std::current_exception());
			pendingReceives.pop();
		}
	}

	if(pendingReceives.size() == 0) {
		pendingReceiveTrackerRunning = false;
	} else {
		io_service.post(&trackAsyncReceive);
	}
}

void trackAsyncProbe() {

	bool rerun = false;
//	std::cout << "pendingProbes:" << std::endl;
//	for(auto& p : pendingProbes) {
//		std::cout << p.first << " count = " << p.second.size() << std::endl;
//	}

	std::vector<Endpoint> emptyQueues;

	for(auto& p : pendingProbes) {
		auto& promiseQueue = p.second;
		while(promiseQueue.size() > 0) {
			try {
				const auto& ep = p.first;

//  				std::cout << "probe on " << ep << std::endl;
				auto headerStatus = world->iprobe(boost::mpi::any_source, ep.second);
				if(headerStatus) {
					//Prepare Header Buffer
					const auto headerSize = headerStatus->count<std::uint8_t>().get();
					std::unique_ptr<std::uint8_t[]> headerBuffer(new std::uint8_t[headerSize]);
					auto headerRequest = world->irecv(headerStatus->source(), headerStatus->tag(), headerBuffer.get(), headerSize);

					// Prepare Body Buffer
					auto bodyStatus = world->probe(headerStatus->source(), ep.second);
					const auto bodySize = bodyStatus.count<Buffer::value_type>().get();
					std::unique_ptr<std::uint8_t[]> bodyBuffer(new std::uint8_t[bodySize]);
					auto bodyRequest = world->irecv(bodyStatus.source(), bodyStatus.tag(), bodyBuffer.get(), bodySize);

					pendingReceives.push(
						PendingReceive {
							std::move(headerRequest),
							std::move(bodyRequest),
							std::move(promiseQueue.front()),
							std::move(headerBuffer),
							std::move(bodyBuffer)
						}
					);

					promiseQueue.pop();

					if(!pendingReceiveTrackerRunning) {
						pendingReceiveTrackerRunning = true;
						io_service.post(&trackAsyncReceive);
					}

				} else {
					rerun = true;
					break;
				}
			} catch(...) {
				promiseQueue.front().set_exception(std::current_exception());
				promiseQueue.pop();
			}
		}
		if(promiseQueue.size() == 0) {
			emptyQueues.emplace_back(p.first);
		}
	}

	for(auto ep : emptyQueues) {
		pendingProbes.erase(ep);
	}

	if(rerun) {
		io_service.post(&trackAsyncProbe);
	} else {
		pendingProbeTrackerRunning = false;
	}
}

void BoostMpiSocket::bind(Endpoint endpoint) {
	if(local != Endpoint(0, 0)) endpointFactory.release(local);

	if(endpoint.second != 0) {
		local = endpoint;
		local.first = endpointFactory.rank;
	} else {
		local = endpointFactory.next();
	}

	endpointFactory.block(local);
}

std::future<void> BoostMpiSocket::asyncSendTo(const ImmutableBuffer& data, const Endpoint remote, const ImmutableBuffer& headerBuffer) {
// 	std::cout << "send to " << remote << std::endl;

	auto promise = std::make_shared<std::promise<void>>();
	auto future = promise->get_future();

	try {

		if(remote.second == 0) {
			bind();
		}

		const auto headerSize = sizeof(local.second) + headerBuffer.size;
		auto header = std::make_shared<std::uint8_t>(headerSize);
		std::memcpy(header.get(), &local.second, sizeof(local.second));
		std::memcpy(header.get() + sizeof(local.second), headerBuffer.data, headerBuffer.size);

		io_service.post([remote, headerSize, header = std::move(header), promise = std::move(promise), data](){
// 			std::cout << "send to " << remote << std::endl;
			auto headerRequest = world->isend(remote.first, remote.second, header.get(), headerSize);
			auto bodyRequest = world->isend(remote.first, remote.second, data.data, data.size);

			pendingSends.push(
				std::make_tuple(
					std::move(headerRequest),
					std::move(bodyRequest),
					std::move(*promise),
					std::move(header)
				)
			);

			// Kickstart send tracker
			if(!pendingSendTrackerRunning) {
				pendingSendTrackerRunning = true;
				io_service.post(&trackAsyncSend);
			}
		});

	} catch(...) {
		promise->set_exception(std::current_exception());
	}


	return future;
}

std::future< cracen2::sockets::BoostMpiSocket::Datagram > cracen2::sockets::BoostMpiSocket::asyncReceiveFrom()
{
	auto promise = std::make_shared<std::promise<Datagram>>();
	auto future = promise->get_future();

	if(local.second == 0) {
		throw std::runtime_error("Trying to receive on closed socket.");
	}

	io_service.post([local = this->local, promise = std::move(promise)](){
		//std::cout << "receive on " << local << std::endl;
		pendingProbes[local].push(std::move(*promise));

		if(!pendingProbeTrackerRunning) {
			pendingProbeTrackerRunning = true;
			io_service.post(&trackAsyncProbe);
		}

	});

	return future;
}

bool BoostMpiSocket::isOpen() const {
	return local != Endpoint();
}

BoostMpiSocket::Endpoint BoostMpiSocket::getLocalEndpoint() const {
	return local;
}

void BoostMpiSocket::close() {
	if(isOpen()) endpointFactory.release(local);
	local = Endpoint(0, 0);
}

EndpointFactory::EndpointFactory() {
};

EndpointFactory::Endpoint EndpointFactory::next() {
	std::unique_lock<std::mutex> lock(mutex);
	Endpoint ep;
	ep.first = rank;
	do {
		ep.second = std::rand() % std::numeric_limits<std::uint16_t>::max();
	} while(blockedEndpoints.count(ep));
	//block(ep);
	return ep;
}

void EndpointFactory::block(const Endpoint& ep) {
	std::unique_lock<std::mutex> lock(mutex);
	blockedEndpoints.insert(ep);
}

void EndpointFactory::release(const Endpoint& ep) {
	std::unique_lock<std::mutex> lock(mutex);
	auto it = std::find(blockedEndpoints.begin(), blockedEndpoints.end(), ep);
	if(it != blockedEndpoints.end()) {
		blockedEndpoints.erase(it);
	} else {
// 		std::stringstream s;
// 		s << "BoostMpiSocket::EndpointFactory: Tried to release a unblocked endpoint.{ rank = " << ep.first << ", tag = " << ep.second << "}\n";
// 		s << "blockedEndpoints = { ";
// 		for(const auto& e : blockedEndpoints) {
// 			s << "{" << e.first << ", " << e.second << "}" << ", ";
// 		}
// 		s << " }\n";
// 		throw(std::runtime_error(s.str()));
	}
}

std::ostream& operator<<(std::ostream& lhs, const cracen2::sockets::BoostMpiSocket::Endpoint& rhs) {
	lhs << rhs.first << ":" << rhs.second;
	return lhs;
}
