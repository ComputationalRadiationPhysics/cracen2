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
	using Endpoint = typename Communicator::Endpoint;

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

	Communicator communicator;
	util::JoiningThread serverThread;

	void serverFunction();

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
	communicator.bind(endpoint);
	serverThread = util::JoiningThread("CracenServer::serverThread", &CracenServer::serverFunction, this);
}

template <class Endpoint>
Endpoint normalize(Endpoint dataEp, Endpoint) {
	return dataEp;
};

template <>
boost::asio::ip::udp::endpoint normalize(boost::asio::ip::udp::endpoint dataEp, boost::asio::ip::udp::endpoint managerEp) {
	if(dataEp.address() == boost::asio::ip::address::from_string("0.0.0.0")) {
		// Participant has specified an interface/ip
		// Set it to the ip, that the server is receiving from
		dataEp.address(managerEp.address());
	}
	return dataEp;
}

template <>
boost::asio::ip::tcp::endpoint normalize(boost::asio::ip::tcp::endpoint dataEp, boost::asio::ip::tcp::endpoint managerEp) {
	if(dataEp.address() == boost::asio::ip::address::from_string("0.0.0.0")) {
		// Participant has specified an interface/ip
		// Set it to the ip, that the server is receiving from
		dataEp.address(managerEp.address());
	}
	return dataEp;
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::serverFunction() {
	std::vector<Endpoint> registerQueue;
	bool running = true;
	auto visitor = Communicator::make_visitor(
		[this, &registerQueue](backend::Register, Endpoint from){
 			std::cout << "Server: Received register, server state = " << static_cast<unsigned int>(state) << std::endl;
			switch(state) {
				case State::ContextUninitialised:
					// First client is connecting
// 					std::cout << "Server: First client connected. Initialising Context..." << std::endl;
					state = State::ContextInizialising;
					communicator.sendTo(backend::RoleGraphRequest(), from);
					registerQueue.push_back(from);
					break;
				case State::ContextInizialising:
					registerQueue.push_back(from);
					break;
				case State::ContextInitialised:
// 					std::cout << "Server: send roles complete" << std::endl;
					// Package was delayed. Send to endpoint from reg package
					communicator.sendTo(backend::RolesComplete(), from);
					break;
			}
		},
		[this](backend::AddRoleConnection addRoleConnection, Endpoint from){
			std::cout << "Server received addRoleConnection " << addRoleConnection.from << "->" << addRoleConnection.to << std::endl;
			roleGraphConnections.left.insert(std::make_pair(addRoleConnection.from, addRoleConnection.to));
			communicator.sendTo(addRoleConnection, from); // Reply the same package as ACK
		},
		[this, &registerQueue](backend::RolesComplete rolesComplete, Endpoint){
			std::cout << "Server: Initialised context. Graph:" << std::endl;
			for(const auto& edge : roleGraphConnections.left) {
				std::cout << "Server: 	" << edge.first << "->" << edge.second << std::endl;
			}
			state = State::ContextInitialised;
// 			std::cout << "Server: send roles complete" << std::endl;
			for(const Endpoint& ep : registerQueue) {
 				std::cout << "Server: send roles complete" << std::endl;
				sendTo(ep, rolesComplete);
			}
		},
		[this](backend::Embody<Endpoint> embody, Endpoint from){
			// Register participant in loca participant map
			const Endpoint managerEndpoint = from;
			Endpoint resolvedEndpoint = normalize(embody.endpoint, managerEndpoint);

			Participant participant;
			participant.dataEndpoint = resolvedEndpoint;
			participant.managerEndpoint = managerEndpoint;
			participant.roleId = embody.roleId;

			// Let new participant know, what neighbours exist already
			executeOnNeighbor(embody.roleId, [this, from](const Participant& participant){
				communicator.sendTo(
					 backend::Announce<Endpoint>{
						participant.dataEndpoint,
						participant.roleId
					},
					from
				);
			});

			// Let other participants know, that a new one wants to enter the graph
			executeOnNeighbor(embody.roleId, [this, &participant](const Participant& neighbour){
				sendTo(neighbour.managerEndpoint, backend::Embody<Endpoint>{
					participant.dataEndpoint,
					participant.roleId
				});
			});

			participants[embody.endpoint] = std::move(participant);
		},
		[this](backend::Disembody<Endpoint> disembody, Endpoint from){
			// This function is easy exploitable, since anyone can disembody anyone else.
			// We do this to enable disembodies of participants, that timeout on data communication
			// We have to trust, that everyone behaves in a good way to make this possible.
//   			std::cout << "Received disembody: " << disembody.endpoint << " from " << communicator.getRemoteEndpoint() << std::endl;
			// send ack
			communicator.sendTo(disembody, from);

			Participant& participant = participants.at(disembody.endpoint);

			// send disembody to all neighbours
			executeOnNeighbor(participant.roleId,[this, &participant](const Participant& neighbour){
				std::cout << "Send disembody to neighbour." << std::endl;
				sendTo(neighbour.managerEndpoint, backend::Disembody<Endpoint>{ participant.dataEndpoint });
			});
		},
		[&running](backend::ServerClose, Endpoint) {
// 			std::cout << "Server received ServerClose. Shutting down." << std::endl;
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
			std::cerr << "Server closing connection because an exception is thrown: "  << e.what() << std::endl;
	}
	std::cout << "Server shutting down." << std::endl;
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::stop() {
	communicator.sendTo(backend::ServerClose(), communicator.getLocalEndpoint());
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
	communicator.sendTo(std::forward<Message>(message), endpoint);
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
	return communicator.getLocalEndpoint();
}

} // End of namespace cracen2
