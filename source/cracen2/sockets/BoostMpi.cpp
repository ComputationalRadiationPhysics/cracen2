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

std::set<BoostMpiSocket::Endpoint> EndpointFactory::blockedEndpoints;

boost::mpi::environment BoostMpiSocket::env(boost::mpi::threading::level::serialized);
boost::mpi::communicator BoostMpiSocket::world;

boost::asio::io_service BoostMpiSocket::io_service;
boost::asio::io_service::work BoostMpiSocket::work(io_service);

JoiningThread BoostMpiSocket::mpiThread([](){ io_service.run(); });

BoostMpiSocket::BoostMpiSocket() {
	// Thread function must be set after io_service::work object
	auto rankPromise = std::make_shared<std::promise<int>>();
	auto rankFuture = rankPromise->get_future();
	io_service.post([rankPromise = std::move(rankPromise)]() {
		rankPromise->set_value(world.rank());
	});

	endpointFactory.rank = rankFuture.get();
};

BoostMpiSocket::~BoostMpiSocket() {
	if(isOpen()) close();
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


void BoostMpiSocket::trackAsyncSend(std::shared_ptr<std::promise<void>> promise, boost::mpi::request request) {
	auto optional = request.test();
	if(optional) {
		promise->set_value();
	} else {
		io_service.post(
			[this, promise = std::move(promise), request = std::move(request)](){
				trackAsyncSend(std::move(promise), std::move(request));
			}
		);
	}
}

std::future<void> BoostMpiSocket::asyncSendTo(const ImmutableBuffer& data, const Endpoint remote) {

	auto promise = std::make_shared<std::promise<void>>();

	io_service.post([this, data, remote, promise]() {
		auto request = world.isend(remote.first, remote.second, data.data, data.size);
		io_service.post(
			[this, promise = std::move(promise), request = std::move(request)](){
				trackAsyncSend(std::move(promise), std::move(request));
			}
		);
	});

	return promise->get_future();
}

void BoostMpiSocket::trackAsyncProbe(std::shared_ptr<std::promise<std::pair<network::Buffer, BoostMpiSocket::Endpoint>>> promise) {
	auto optional = world.iprobe();
	if(optional) {
		auto status = optional.get();
		auto buffer = std::make_shared<Buffer::Base>(status.count<Buffer::Base::value_type>().get());
		auto request = world.irecv(status.source(), status.tag(), buffer->data(), buffer->size());
		io_service.post([this, promise = std::move(promise), buffer = std::move(buffer), request = std::move(request)](){
			trackAsyncReceive(std::move(promise), std::move(buffer), std::move(request));
		});
	} else {
		io_service.post([this, promise = std::move(promise)](){
			trackAsyncProbe(std::move(promise));
		});
	}

}

void BoostMpiSocket::trackAsyncReceive(std::shared_ptr<std::promise<std::pair<network::Buffer, BoostMpiSocket::Endpoint>>> promise, std::shared_ptr<Buffer::Base> buffer, boost::mpi::request request) {
	auto optional = request.test();
	if(optional) {
		std::cout << "receive complete" << std::endl;
		auto status = optional.get();
		promise->set_value(std::make_pair(std::move(*buffer), std::make_pair(status.source(), status.tag())));
	} else {
		io_service.post(
			[this, promise = std::move(promise), buffer = std::move(buffer), request = std::move(request)](){
				trackAsyncReceive(std::move(promise), std::move(buffer), std::move(request));
			}
		);
	}

}

std::future<std::pair<Buffer, BoostMpiSocket::Endpoint>>  BoostMpiSocket::asyncReceiveFrom() {

	auto promise = std::make_shared<std::promise<std::pair<Buffer, BoostMpiSocket::Endpoint>>>();

	io_service.post([this, promise](){
		trackAsyncProbe(std::move(promise));
	});

	return promise->get_future();
}

bool BoostMpiSocket::isOpen() const {
	return local != Endpoint();
}

BoostMpiSocket::Endpoint BoostMpiSocket::getLocalEndpoint() const {
	return local;
}

void BoostMpiSocket::close() {
	endpointFactory.release(local);
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
		std::cout << ep.first << ", " << ep.second << " released." << std::endl;
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
