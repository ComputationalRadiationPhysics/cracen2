#pragma once

#include <map>
#include <queue>
#include <set>
#include <future>
#include <boost/asio/io_service.hpp>
#include <boost/mpi.hpp>

#include "cracen2/network/ImmutableBuffer.hpp"
#include "cracen2/util/Thread.hpp"

namespace cracen2 {

namespace sockets {

namespace detail {

class EndpointFactory {
public:
	using Endpoint = std::pair<int, int>;
	int rank;

	EndpointFactory();
	Endpoint next();
	void block(const Endpoint&);
	void release(const Endpoint&);

private:
	static std::mutex mutex;
	static std::set<Endpoint> blockedEndpoints;

}; // End of class EndpointFactory

} // End of namespace detail

class BoostMpiSocket {

	friend class detail::EndpointFactory;

public:

	using Endpoint = detail::EndpointFactory::Endpoint;
	struct Datagram {
		network::Buffer header;
		network::Buffer body;
		Endpoint remote;
	};

private:

	using ImmutableBuffer = network::ImmutableBuffer;

	detail::EndpointFactory endpointFactory;

	Endpoint local;

public:

	BoostMpiSocket();
	~BoostMpiSocket();

	BoostMpiSocket(BoostMpiSocket&& other) = default;
	BoostMpiSocket& operator=(BoostMpiSocket&& other) = default;

	BoostMpiSocket(const BoostMpiSocket& other) = delete;
	BoostMpiSocket& operator=(const BoostMpiSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint());

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote, const ImmutableBuffer& header = ImmutableBuffer(nullptr, 0));
	std::future<Datagram> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class BoostMpiSocket

} // End of namespace sockets

} // End of namespace cracen2

std::ostream& operator<<(std::ostream& lhs, const cracen2::sockets::BoostMpiSocket::Endpoint& rhs);
