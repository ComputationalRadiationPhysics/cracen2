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
	using Datagram = std::pair<network::Buffer, Endpoint>;

private:

	using ImmutableBuffer = network::ImmutableBuffer;

	detail::EndpointFactory endpointFactory;

	Endpoint local;

	static std::map<
		Endpoint,
		std::queue<
			std::promise<Datagram>
		>
	> pendingProbes;
	static bool pendingProbeTrackerRunning;

	static std::queue<
		std::tuple<
			boost::mpi::request,
			std::promise<Datagram>,
			std::unique_ptr<std::uint8_t[]>
		>
	> pendingReceives;
	static bool pendingReceiveTrackerRunning;

	static std::queue<
		std::tuple<
			boost::mpi::request,
			std::promise<void>,
			std::shared_ptr<std::uint8_t>
		>
	> pendingSends;
	static bool pendingSendTrackerRunning;

	static std::unique_ptr<boost::mpi::environment> env;
	static std::unique_ptr<boost::mpi::communicator> world;

	static util::JoiningThread mpiThread;

	static boost::asio::io_service io_service;

	static boost::asio::io_service::work work;

	static void trackAsyncReceive();
	static void trackAsyncSend();
	static void trackAsyncProbe();

public:

	BoostMpiSocket();
	~BoostMpiSocket();

	BoostMpiSocket(BoostMpiSocket&& other) = default;
	BoostMpiSocket& operator=(BoostMpiSocket&& other) = default;

	BoostMpiSocket(const BoostMpiSocket& other) = delete;
	BoostMpiSocket& operator=(const BoostMpiSocket& other) = delete;

	void bind(Endpoint endpoint = Endpoint());

	std::future<void> asyncSendTo(const ImmutableBuffer& data, const Endpoint remote);
	std::future<Datagram> asyncReceiveFrom();

	bool isOpen() const;
	Endpoint getLocalEndpoint() const;

	void close();

}; // End of class BoostMpiSocket

} // End of namespace sockets

} // End of namespace cracen2

