#include "cracen2/sockets/BoostMpi.hpp"

#include <cstdlib>
#include <ctime>
#include <limits>
#include <cstring>
#include <cstdint>
#include <boost/serialization/vector.hpp>

using namespace cracen2::util;
using namespace cracen2::sockets;
using namespace cracen2::network;
using namespace cracen2::sockets::detail;

using Datagram = BoostMpiSocket::Datagram;

std::map<BoostMpiSocket::Endpoint,std::vector<std::promise<Datagram>>> BoostMpiSocket::pendingProbes;
std::queue<std::tuple<boost::mpi::request,std::promise<Datagram>, std::unique_ptr<Buffer::Base>>> BoostMpiSocket::pendingReceives;
std::queue<std::pair<boost::mpi::request,std::promise<void>>> BoostMpiSocket::pendingSends;

bool BoostMpiSocket::pendingProbeTrackerRunning = false;
bool BoostMpiSocket::pendingReceiveTrackerRunning = false;
bool BoostMpiSocket::pendingSendTrackerRunning = false;

std::set<BoostMpiSocket::Endpoint> EndpointFactory::blockedEndpoints;

std::unique_ptr<boost::mpi::environment> BoostMpiSocket::env;
std::unique_ptr<boost::mpi::communicator> BoostMpiSocket::world;

boost::asio::io_service BoostMpiSocket::io_service;

JoiningThread BoostMpiSocket::mpiThread([](){

	env = std::make_unique<boost::mpi::environment>(boost::mpi::threading::funneled);
	world = std::make_unique<boost::mpi::communicator>();

	io_service.run();

});

boost::asio::io_service::work BoostMpiSocket::work(io_service);



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


void BoostMpiSocket::trackAsyncSend() {
	while(pendingSends.size() > 0) {
		auto& p = pendingSends.front();
		try {
			auto status = p.first.test();
			if(status) {
				// Send completed
				p.second.set_value();
				pendingSends.pop();
			} else {
				break;
			}
		} catch(...) {
			p.second.set_exception(std::current_exception());
			pendingSends.pop();
		}
	}

	if(pendingSends.size() == 0) {
		pendingSendTrackerRunning = false;
	} else {
		io_service.post(&BoostMpiSocket::trackAsyncSend);
	}
}

void BoostMpiSocket::trackAsyncReceive() {

	while(pendingReceives.size() > 0) {
		auto& t = pendingReceives.front();
		try {
		auto& request = std::get<boost::mpi::request>(t);
		auto status = request.test();
		if(status) {
			auto& promise = std::get<std::promise<Datagram>>(t);
			auto& buffer = std::get<std::unique_ptr<Buffer::Base>>(t);
			promise.set_value(
				Datagram(
					std::move(*buffer),
					std::make_pair(
						status->source(),
						status->tag()
					)
				)
			);
			pendingReceives.pop();
		} else {
			break;
		}
		} catch(...) {
			auto& promise = std::get<std::promise<Datagram>>(t);
			promise.set_exception(std::current_exception());
			pendingReceives.pop();
		}
	}

	if(pendingReceives.size() == 0) {
		pendingReceiveTrackerRunning = false;
	} else {
		io_service.post(&BoostMpiSocket::trackAsyncReceive);
	}
}

void BoostMpiSocket::trackAsyncProbe() {

	bool rerun = false;

	for(auto& p : pendingProbes) {
		auto& promiseVector = p.second;
		while(promiseVector.size() > 0) {
			try {
				const auto& ep = p.first;

				auto status = world->iprobe(ep.first, ep.second);
				if(status) {
					auto buffer = std::make_unique<Buffer::Base>(status->count<Buffer::Base::value_type>().get());
					auto request = world->irecv(status->source(), status->tag(), buffer->data(), buffer->size());

					pendingReceives.push(
						std::make_tuple(
							std::move(request),
							std::move(promiseVector.back()),
							std::move(buffer)
						)
					);

					promiseVector.pop_back();

					if(!pendingReceiveTrackerRunning) {
						pendingReceiveTrackerRunning = true;
						io_service.post(&BoostMpiSocket::trackAsyncReceive);
					}

				} else {
					rerun = true;
					break;
				}
			} catch(...) {
				promiseVector.back().set_exception(std::current_exception());
				promiseVector.pop_back();
			}
		}
	}

	if(rerun) {
		io_service.post(&BoostMpiSocket::trackAsyncProbe);
	} else {
		pendingProbeTrackerRunning = false;
	}
}

void BoostMpiSocket::bind(Endpoint endpoint) {
	if(local != Endpoint()) endpointFactory.release(local);

	if(endpoint.second != 0) {
		local = endpoint;
	} else {
		local = endpointFactory.next();
	}

	endpointFactory.block(local);
}

std::future<void> BoostMpiSocket::asyncSendTo(const ImmutableBuffer& data, const Endpoint remote) {

	auto promise = std::make_shared<std::promise<void>>();
	auto future = promise->get_future();

 	io_service.post([remote, data, promise = std::move(promise)](){

		auto request = world->isend(remote.first, remote.second, data.data, data.size);
		pendingSends.push(
			std::make_pair(
				std::move(request),
				std::move(*promise)
			)
		);

		// Kickstart send tracker
		if(!pendingSendTrackerRunning) {
			pendingSendTrackerRunning = true;
			io_service.post(&BoostMpiSocket::trackAsyncSend);
		}
	});

	return future;
}

std::future< cracen2::sockets::BoostMpiSocket::Datagram > cracen2::sockets::BoostMpiSocket::asyncReceiveFrom()
{
	auto promise = std::make_shared<std::promise<Datagram>>();
	auto future = promise->get_future();

	io_service.post([this, promise = std::move(promise)](){

		pendingProbes[local].emplace_back(std::move(*promise));

		if(!pendingProbeTrackerRunning) {
			pendingProbeTrackerRunning = true;
			io_service.post(&BoostMpiSocket::trackAsyncProbe);
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
	local = Endpoint();
}


EndpointFactory::EndpointFactory() {
	std::srand(std::time(0));
};

EndpointFactory::Endpoint EndpointFactory::next() {
	Endpoint ep;
	ep.first = rank;
	do {
		ep.second = std::rand() % std::numeric_limits<std::uint16_t>::max();
	} while(blockedEndpoints.count(ep));
	block(ep);
	return ep;
}

void EndpointFactory::block(const Endpoint& ep) {
	blockedEndpoints.insert(ep);
}

void EndpointFactory::release(const Endpoint& ep) {
	auto it = std::find(blockedEndpoints.begin(), blockedEndpoints.end(), ep);
	if(it != blockedEndpoints.end()) {
		blockedEndpoints.erase(it);
	} else {
		std::stringstream s;
		s << "BoostMpiSocket::EndpointFactory: Tried to release a unblocked endpoint.{ rank = " << ep.first << ", tag = " << ep.second << "}\n";
		s << "blockedEndpoints = { ";
		for(const auto& e : blockedEndpoints) {
			s << "{" << e.first << ", " << e.second << "}" << ", ";
		}
		s << " }\n";
		throw(std::runtime_error(s.str()));
	}
}
