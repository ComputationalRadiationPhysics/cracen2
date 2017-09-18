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
#include "cracen2/util/AtomicQueue.hpp"

#include <boost/asio.hpp>

namespace cracen2 {

template <class SocketImplementation>
class CracenServer {
public:

	using TagList = typename backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using Communicator = network::Communicator<SocketImplementation, TagList>;
	using Acceptor = typename Communicator::Acceptor;
	using Endpoint = typename Communicator::Endpoint;
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
	GraphConnectionType roleGraphConnections;
	ParticipantMapType participants;

	Acceptor acceptor;
	util::JoiningThread acceptorThread;
	std::mutex serverMutex;

	std::vector<std::unique_ptr<Communicator>> communicators;
	std::vector<util::JoiningThread> serverThreads;

	void acceptorFunction();
	void serverFunction(std::size_t id);

	template<class Callable>
	void executeOnRole(const backend::RoleId& roleId, Callable callable);
	template<class RoleRange, class Callable>
	void executeOnRange(RoleRange range, Callable callable);
	template<class Callable>
	void executeOnNeighbor(const backend::RoleId& role, Callable callable);

	template<class Message>
	void sendTo(const Endpoint& endpoint, Message&& message);

public:

	CracenServer(Endpoint endpoint = Endpoint());
	void stop();

	void printStatus() const;

	Endpoint getEndpoint();

}; // End of class CracenServer

template <class SocketImplementation>
CracenServer<SocketImplementation>::CracenServer(CracenServer::Endpoint endpoint) :
	state(State::ContextUninitialised)
{
	acceptor.bind(endpoint);
	acceptorThread = util::JoiningThread(&CracenServer::acceptorFunction, this);
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::acceptorFunction() {
	while(acceptor.isOpen()) {
		try{
			const auto id = communicators.size();
			communicators.emplace_back(new Communicator(acceptor.accept()));
			serverThreads.emplace_back(
				&CracenServer::serverFunction,
				this,
				id
			);
		} catch(const std::exception& e) {
			std::cerr << "Acceptor threw an exception: " << e.what() << std::endl;
			break;
		}
	}
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::serverFunction(std::size_t id) {
	Communicator& communicator = *communicators[id];

	std::vector<Endpoint> registerQueue;
	bool running = true;
	Visitor visitor(
		[this, &communicator, &registerQueue](backend::Register){
			std::unique_lock<std::mutex> lock(serverMutex);
// 			std::cout << "Server: Received register, server state = " << static_cast<unsigned int>(state) << std::endl;
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
			std::unique_lock<std::mutex> lock(serverMutex);
			roleGraphConnections.left.insert(std::make_pair(addRoleConnection.from, addRoleConnection.to));
			communicator.send(addRoleConnection); // Reply the same package as ACK
		},
		[this, &communicator, &registerQueue](backend::RolesComplete rolesComplete){
			std::unique_lock<std::mutex> lock(serverMutex);
			// 			std::cout << "Server: Initialised context. Graph:" << std::endl;
// 			for(const auto& edge : roleGraphConnections.left) {
// 				std::cout << "Server: 	" << edge.first << "->" << edge.second << std::endl;
// 			}
			state = State::ContextInitialised;
// 			std::cout << "Server: send roles complete" << std::endl;
			for(const Endpoint& ep : registerQueue) {
				sendTo(ep, rolesComplete);
			}
		},
		[this, &communicator](backend::Embody<Endpoint> embody){
			std::unique_lock<std::mutex> lock(serverMutex);
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
			executeOnNeighbor(embody.roleId, [this, &communicator, &participant](const Participant& neighbour){
				sendTo(neighbour.managerEndpoint, backend::Embody<Endpoint>{
					participant.dataEndpoint,
					participant.roleId
				});
			});

			participants[embody.endpoint] = std::move(participant);
		},
		[this, &communicator](backend::Disembody<Endpoint> disembody){
			std::unique_lock<std::mutex> lock(serverMutex);
			// This function is easy exploitable, since anyone can disembody anyone else.
			// We do this to enable disembodies of participants, that timeout on data communication
			// We have to trust, that everyone behaves in a good way to make this possible.
// 			std::cout << "Received disembody: " << disembody.endpoint << " from " << communicator.getRemoteEndpoint() << std::endl;
			// send ack
			communicator.send(disembody);

			Participant& participant = participants.at(disembody.endpoint);

			// send disembody to all neighbours
			executeOnNeighbor(participant.roleId,[this, &communicator, &participant](const Participant& neighbour){
				sendTo(neighbour.managerEndpoint, backend::Disembody<Endpoint>{ participant.dataEndpoint });
			});
		},
		[this, &running](backend::ServerClose) {
			std::unique_lock<std::mutex> lock(serverMutex);
			running = false;
		}
	);

	try {
		std::stringstream s;
		s << "Server receiving on " << communicator.getLocalEndpoint() << std::endl;
		std::cout << s.rdbuf() << std::endl;
		while(running) {
			communicator.receive(visitor);
		}
	} catch(const std::exception& e) {
	//		std::cerr << "Server closing connection because an exception is thrown: "  << e.what() << std::endl;
	}
	std::cout << "Server shutting down." << std::endl;
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::stop() {
	std::cout << "Shutting down server." << std::endl;
	acceptor.close();
	if(!SocketImplementation::fixedRemoteEndpoint) {
		Communicator com;
		com.connect(communicators.front()->getLocalEndpoint());
		com.send(backend::ServerClose());
	}
 	for(auto& c : communicators) {
		c->close();
	}
}

template <class SocketImplementation>
template<class Callable>
void CracenServer<SocketImplementation>::executeOnRole(const backend::RoleId& roleId, Callable callable) {
	for(auto& participant_pair : participants) {	std::stringstream s;
// 	s << "Server receiving on " << communicator.getLocalEndpoint() << std::endl;
// 	std::cout <<
		if(participant_pair.second.roleId == roleId) {
			callable(participant_pair.second);
		}
	}
}

template <class SocketImplementation>
template<class RoleRange, class Callable>
void CracenServer<SocketImplementation>::executeOnRange(RoleRange range, Callable callable) {
	for(
		auto senderIter = range.first;
		senderIter != range.second;
		senderIter++
	) {
		executeOnRole(senderIter->second, callable);
	}
}

template <class SocketImplementation>
template<class Callable>
void CracenServer<SocketImplementation>::executeOnNeighbor(const backend::RoleId& role, Callable callable) {
	executeOnRange(roleGraphConnections.left.equal_range(role), callable);
	executeOnRange(roleGraphConnections.right.equal_range(role), callable);
}

template <class SocketImplementation>
template<class Message>
void CracenServer<SocketImplementation>::sendTo(const Endpoint& endpoint, Message&& message) {
	if(SocketImplementation::fixedRemoteEndpoint == true) {
		for(auto& communicator : communicators) {
			if(communicator->getRemoteEndpoint() == endpoint) {
				communicator->send(std::forward<Message>(message));
				break;
			}
		}
	} else {
		auto& communicator = communicators.front();
		communicator->connect(endpoint);
		communicator->send(std::forward<Message>(message));
	}
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::printStatus() const {
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

template <class SocketImplementation>
typename CracenServer<SocketImplementation>::Endpoint CracenServer<SocketImplementation>::getEndpoint() {
	return acceptor.getLocalEndpoint();
}

} // End of namespace cracen2
