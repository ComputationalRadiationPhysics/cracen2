#pragma once

#include <boost/mpi.hpp>
#include <set>
#include <mutex>

#include "cracen2/network/ImmutableBuffer.hpp"

namespace cracen2 {

namespace sockets {

namespace detail {

class EndpointFactory {
public:
	using Endpoint = std::pair<int, int>;

	EndpointFactory();
	Endpoint next();
	void block(const Endpoint&);
	void release(const Endpoint&);

private:
	std::set<Endpoint> blockedEndpoints;

}; // End of class EndpointFactory

} // End of namespace detail

class BoostMpiSocket {
	friend class detail::EndpointFactory;

public:
	using Endpoint = detail::EndpointFactory::Endpoint;
private:

	using ImmutableBuffer = network::ImmutableBuffer;

	static boost::mpi::environment env;
	static boost::mpi::communicator world;
	static std::mutex sendMutex;

	static detail::EndpointFactory endpointFactory;

	Endpoint local;
	Endpoint remote;

public:



	BoostMpiSocket();
	~BoostMpiSocket();

	BoostMpiSocket(BoostMpiSocket&& other);
	BoostMpiSocket& operator=(BoostMpiSocket&& other);

	BoostMpiSocket(const BoostMpiSocket& other) = delete;
	BoostMpiSocket& operator=(const BoostMpiSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint());
	void accept();
	void connect(Endpoint destination);
	void send(const ImmutableBuffer& data);
	network::Buffer receive();
	bool isOpen() const;
	Endpoint getLocalEndpoint() const;
	Endpoint getRemoteEndpoint() const;

	void close();

}; // End of class BoostMpiSocket

} // End of namespace sockets

} // End of namespace cracen2

