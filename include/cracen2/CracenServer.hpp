#pragma once

#include <atomic>
#include <future>
#include <iostream>
#include <map>

#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "cracen2/util/Thread.hpp"
#include "cracen2/network/Communicator.hpp"
#include "cracen2/backend/Messages.hpp"

#include <boost/asio.hpp>

namespace cracen2 {

template <class SocketImplementation>
class CracenServer {
public:

	using TagList = typename backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using Communicator = network::Communicator<SocketImplementation, TagList>;
	using Endpoint = typename Communicator::Endpoint;
	using Port = typename Communicator::Port;
	using Visitor = typename Communicator::Visitor;

	struct Participant {
		// Endpoint unresolvedEndpoint; // Key in Participant map
		Endpoint dataEndpoint;
		Endpoint managerEndpoint;
		backend::RoleId roleId;
	};

	using GraphConnectionType = boost::bimap<
		boost::bimaps::multiset_of<backend::RoleId>,
		boost::bimaps::multiset_of<backend::RoleId>
	>;

	using ParticipantMapType = std::map<
		Endpoint,
		Participant
	>;

	enum class State {
		ContextUninitialised,
		ContextInizialising,
		ContextInitialised
	};

private:

	State state;
	Endpoint serverEndpoint;
	GraphConnectionType roleGraphConnections;
	ParticipantMapType participants;

	std::promise<bool> runningPromise;
	std::atomic<bool> running;
	util::JoiningThread serverThread;
	void run(Port port);

	template<class Callable>
	void executeOnRole(const backend::RoleId& roleId, Callable callable);
	template<class RoleRange, class Callable>
	void executeOnRange(RoleRange range, Callable callable) {
		for(
			auto senderIter = range.first;
			senderIter != range.second;
			senderIter++
		) {
			executeOnRole(senderIter->second, callable);
		}
	}

	template<class Callable>
	void executeOnNeighbor(const backend::RoleId& role, Callable callable) {
		executeOnRange(roleGraphConnections.left.equal_range(role), callable);
		executeOnRange(roleGraphConnections.right.equal_range(role), callable);
	}

public:
	/*
	 * TODO: Using a port as argument to start the server is a bad design choice:
	 * - It does not let you choose an interface
	 * - It is inconsistent with other backends, since port may only exist for tcp and udp
	 */

	CracenServer(Port port);
	void stop();

	void printStatus() const {
		std::stringstream status;
		status
			<< "Status for cracen server:\n"
			//<< "	communication endpoint: " << communicator.getLocalEndpoint() << "\n"
			<< "	RoleGraphConnections:[\n";
			for(const auto& con : roleGraphConnections.left) {
				status << "		" << con.first << " -> " << con.second << "\n";
			}
		status
			<< "	]\n"
			<< "	Participants;[\n";
			for(const auto& pp : participants) {
				status << "		" << pp.first << " { " << pp.second.roleId << " }\n";
			}
		status
			<< "	]\n";
		std::cout << status.rdbuf();
	}

};

template <class SocketImplementation>
CracenServer<SocketImplementation>::CracenServer(CracenServer::Port port) :
	state(State::ContextUninitialised),
	running(true),
	serverThread(&CracenServer::run, this, port)
{
	if(runningPromise.get_future().get() == false)
		throw(std::runtime_error("Could not start and bind communicator."));
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::run(CracenServer::Port port) {
	Communicator communicator;
	std::vector<Endpoint> registerQueue;

	Visitor visitor(
		[this, &communicator, &registerQueue](backend::Register){
			//std::cout << "Server: Received register, server state = " << static_cast<unsigned int>(state) << std::endl;
			switch(state) {
				case State::ContextUninitialised:
					// First client is connecting
// 					std::cout << "Server: First client connected. Initialising Context..." << std::endl;
					state = State::ContextInizialising;
					communicator.send(backend::RoleGraphRequest());
					registerQueue.push_back(communicator.getRemoteEndpoint());
					break;
				case State::ContextInizialising:
					registerQueue.push_back(communicator.getRemoteEndpoint());
					break;
				case State::ContextInitialised:
// 					std::cout << "Server: send roles complete" << std::endl;
					// Package was delayed. Send to endpoint from reg package
					communicator.send(backend::RolesComplete());
					break;
			}
		},
		[this, &communicator](backend::AddRoleConnection addRoleConnection){
			roleGraphConnections.left.insert(std::make_pair(addRoleConnection.from, addRoleConnection.to));
			communicator.send(addRoleConnection); // Reply the same package as ACK
		},
		[this, &communicator, &registerQueue](backend::RolesComplete rolesComplete){
// 			std::cout << "Server: Initialised context. Graph:" << std::endl;
// 			for(const auto& edge : roleGraphConnections.left) {
// 				std::cout << "Server: 	" << edge.first << "->" << edge.second << std::endl;
// 			}
			state = State::ContextInitialised;
// 			std::cout << "Server: send roles complete" << std::endl;
			for(const Endpoint& ep : registerQueue) {
				communicator.connect(ep);
				communicator.send(rolesComplete);
			}
		},
		[this, &communicator](backend::Embody<Endpoint> embody){
			// Register participant in loca participant map
			Endpoint resolvedEndpoint = embody.endpoint;
			const Endpoint managerEndpoint = communicator.getRemoteEndpoint();
			if(resolvedEndpoint.address() == boost::asio::ip::address::from_string("0.0.0.0")) {
				// Participant has specified an interface/ip
				// Set it to the ip, that the server is receiving from
				resolvedEndpoint.address(managerEndpoint.address());
			}

			Participant participant;
			participant.dataEndpoint = resolvedEndpoint;
			participant.managerEndpoint = managerEndpoint;
			participant.roleId = embody.roleId;

			// Let new participant know, what neighbours exist already
			executeOnNeighbor(embody.roleId, [&communicator](const Participant& participant){
				communicator.send(
					 backend::Announce<Endpoint>{
						participant.dataEndpoint,
						participant.roleId
					}
				);
			});

			// Let other participants know, that a new one wants to enter the graph
			executeOnNeighbor(embody.roleId, [&communicator, &participant](const Participant& neighbour){
				communicator.connect(neighbour.managerEndpoint);
				communicator.send(
					 backend::Embody<Endpoint>{
						participant.dataEndpoint,
						participant.roleId
					}
				);
			});

			participants[embody.endpoint] = std::move(participant);
		},
		[this, &communicator](backend::Disembody<Endpoint> disembody){
			// This function is easy exploitable, since anyone can disembody anyone else.
			// We do this to enable disembodies of participants, that timeout on data communication
			// We have to trust, that everyone behaves in a good way to make this possible.
// 			std::cout << "Received disembody: " << disembody.endpoint << " from " << communicator.getRemoteEndpoint() << std::endl;
			// send ack
			communicator.send(disembody);

			Participant& participant = participants.at(disembody.endpoint);

			// send disembody to all neighbours
			executeOnNeighbor(participant.roleId,[&communicator, &participant](const Participant& neighbour){
				communicator.connect(neighbour.managerEndpoint);
				communicator.send(backend::Disembody<Endpoint>{ participant.dataEndpoint });
			});
		},
		[this](backend::ServerClose) {
			running = false;
		}
	);

	try {
		communicator.bind(port);
		serverEndpoint = communicator.getLocalEndpoint();
		runningPromise.set_value(true);
		std::cout << "Cracen Server running on " << serverEndpoint << std::endl;
		communicator.accept();
	} catch(const std::exception& e) {
		runningPromise.set_value(false);
		std::cerr << e.what() << std::endl;;
	}

	try {
		while(running) {
			communicator.receive(visitor);
		}
	} catch(const std::exception& e) {
			std::cerr << "Server: Closing connection: "  << e.what() << std::endl;
	}
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::stop() {
	Communicator communicator;
	communicator.connect(serverEndpoint);
	communicator.send(backend::ServerClose());
}

template <class SocketImplementation>
template<class Callable>
void CracenServer<SocketImplementation>::executeOnRole(const backend::RoleId& roleId, Callable callable) {
	for(const auto& participant_pair : participants) {
		if(participant_pair.second.roleId == roleId) {
			callable(participant_pair.second);
		}
	}
}

} // End of namespace cracen2
