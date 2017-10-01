#pragma once

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
	static std::set<Endpoint> blockedEndpoints;

}; // End of class EndpointFactory

} // End of namespace detail

class BoostMpiSocket {

	friend class detail::EndpointFactory;

public:

	using Endpoint = detail::EndpointFactory::Endpoint;

private:

	using ImmutableBuffer = network::ImmutableBuffer;

	detail::EndpointFactory endpointFactory;

	Endpoint local;

	static util::JoiningThread mpiThread;

	static boost::mpi::environment env;
	static boost::mpi::communicator world;

	static boost::asio::io_service io_service;
	static boost::asio::io_service::work work;

	void trackAsyncSend(std::shared_ptr<std::promise<void>> promise, boost::mpi::request request);
	void trackAsyncProbe(std::shared_ptr<std::promise<std::pair<network::Buffer, BoostMpiSocket::Endpoint>>> promise);
	void trackAsyncReceive(std::shared_ptr<std::promise<std::pair<network::Buffer, BoostMpiSocket::Endpoint>>> promise, std::shared_ptr<network::Buffer::Base> buffer, boost::mpi::request request);

public:

	BoostMpiSocket();
	~BoostMpiSocket();

	BoostMpiSocket(BoostMpiSocket&& other) = default;
	BoostMpiSocket& operator=(BoostMpiSocket&& other) = default;

	BoostMpiSocket(const BoostMpiSocket& other) = delete;
	BoostMpiSocket& operator=(const BoostMpiSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint());

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote);
	std::future<std::pair<network::Buffer, Endpoint>> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class BoostMpiSocket

} // End of namespace sockets

} // End of namespace cracen2

