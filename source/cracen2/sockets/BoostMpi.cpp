#include "cracen2/sockets/BoostMpi.hpp"

#include <cstdlib>
#include <ctime>
#include <limits>
#include <cstring>
#include <cstdint>
#include <boost/serialization/vector.hpp>

using namespace cracen2::sockets;
using namespace cracen2::network;
using namespace cracen2::sockets::detail;

boost::mpi::environment BoostMpiSocket::env(boost::mpi::threading::level::multiple);
boost::mpi::communicator BoostMpiSocket::world;
std::mutex BoostMpiSocket::sendMutex;

EndpointFactory BoostMpiSocket::endpointFactory;

BoostMpiSocket::BoostMpiSocket() = default;
BoostMpiSocket::~BoostMpiSocket() = default;

BoostMpiSocket::BoostMpiSocket(BoostMpiSocket&& other) = default;
BoostMpiSocket& BoostMpiSocket::operator=(BoostMpiSocket&& other) = default;


void BoostMpiSocket::bind(Endpoint endpoint) {
	if(local != Endpoint()) endpointFactory.release(local);

	if(endpoint.second != 0) {
		local = endpoint;
	} else {
		local = endpointFactory.next();
	}

	endpointFactory.block(local);
}

void BoostMpiSocket::accept() {
	// No op
}

void BoostMpiSocket::connect(Endpoint destination) {

	if(!isOpen()) {
		bind();
	}

	remote = destination;
}

void BoostMpiSocket::send(const ImmutableBuffer& data) {
	Buffer::Base buffer(data.size + sizeof(int));
	std::memcpy(buffer.data(), data.data, data.size);
	std::memcpy(buffer.data() + data.size, &local.second, sizeof(int));
	std::unique_lock<std::mutex> lock(sendMutex);
	world.send(remote.first, remote.second, buffer);
}

Buffer BoostMpiSocket::receive() {
	auto probe = world.probe(boost::mpi::any_source, local.second);
	if(probe.error() != 0) {
		throw(std::runtime_error("BoostMpiSocket::receive(): boost mpi receive return error code " + std::to_string(probe.error()) + "."));
	}

// 	std::cout << "source = " << probe.source() << " tag = " << probe.tag() << " size = " << probe.count<Buffer::Base::value_type>().get() << std::endl;
	Buffer::Base buffer(probe.count<Buffer::Base::value_type>().get());
// 	std::cout << "buffer size = " << buffer.size() << std::endl;
	auto status = world.recv(probe.source(), probe.tag(), buffer);

	if(status.error() != 0) {
		throw(std::runtime_error("BoostMpiSocket::receive(): boost mpi receive return error code " + std::to_string(status.error()) + "."));
	}
	remote.first = status.source();
	const auto tagPos = buffer.data() + buffer.size() - sizeof(int);
	if(buffer.size() < sizeof(int)) {
		throw(std::runtime_error("BoostMpiSocket::receive(): received no message."));
	}
	remote.second = *reinterpret_cast<int*>(tagPos);
	buffer.resize(buffer.size() - sizeof(int));
	return buffer;
}

bool BoostMpiSocket::isOpen() const {
	return local != Endpoint();
}

BoostMpiSocket::Endpoint BoostMpiSocket::getLocalEndpoint() const {
	return local;
}

BoostMpiSocket::Endpoint BoostMpiSocket::getRemoteEndpoint() const {
	return remote;
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
	ep.first = BoostMpiSocket::world.rank();
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
