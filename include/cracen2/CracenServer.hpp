#pragma once

#include <atomic>
#include <future>
#include <iostream>

#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "cracen2/util/Thread.hpp"
#include "cracen2/network/Communicator.hpp"
#include "cracen2/backend/Messages.hpp"

namespace cracen2 {

template <class SocketImplementation>
class CracenServer {
public:
	using TagList = typename backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using Communicator = network::Communicator<SocketImplementation, TagList>;
	using Endpoint = typename Communicator::Endpoint;
	using Port = typename Communicator::Port;
	using Visitor = typename Communicator::Visitor;

	using GraphConnectionType = boost::bimap<
		boost::bimaps::multiset_of<backend::RoleId>,
		boost::bimaps::multiset_of<backend::RoleId>
	>;

	using ParticipantMapType = boost::bimap<
		boost::bimaps::set_of<Endpoint>,
		boost::bimaps::multiset_of<backend::RoleId>
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

	template <class Message>
	void sendToRole(Communicator& communicator, const backend::RoleId& role, const Message& message) const {
		//Get all endpoints, that embody that roleId
		const auto participantRange = participants.right.equal_range(role);
		for(
			auto participantIter = participantRange.first;
			participantIter != participantRange.second;
			participantIter++
		) {
			const Endpoint& ep = participantIter->second;
			try {
				communicator.connect(ep);
				communicator.send(message);
			} catch(const std::exception e) {
				std::cerr << "Problem sending message to participant:" << std::endl;
				std::cerr << e.what() << std::endl;
			}
		}
	}

	template <class Message>
	void sendToNeighbor(Communicator& communicator, const backend::RoleId& role, const Message& message) const {

		const auto senderRange = roleGraphConnections.right.equal_range(role);
		for(
			auto senderIter = senderRange.first;
			senderIter != senderRange.second;
			senderIter++
		) {
			sendToRole(communicator, senderIter->second, message);
		}

		const auto receiverRange = roleGraphConnections.left.equal_range(role);
		for(
			auto receiverIter = receiverRange.first;
			receiverIter != receiverRange.second;
			receiverIter++
		) {
			sendToRole(communicator, receiverIter->second, message);
		}
	}

public:

	CracenServer(Port port);
	void stop();

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
		[this, &communicator, &registerQueue](backend::Register<Endpoint>){
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
			const Endpoint embodyEp = communicator.getRemoteEndpoint();
// 			std::cout << "Server: Receive embody { " << embodyEp << ", " << embody.roleId << " }" << std::endl;

			participants.left.insert(std::make_pair(embodyEp, embody.roleId));
			// Let other participants know, that a new one wants to enter the graph
			try {

				backend::RoleId embodyRoleId = participants.left.at(embodyEp);

				sendToNeighbor(communicator, embodyRoleId, backend::Embody<Endpoint>{embodyEp, embody.roleId});

			} catch(const std::exception& e) {
				std::cerr << "Participant, that is not registered wants to embody a role." << std::endl;
				std::cerr << e.what() << std::endl;
			}
		},
		[this, &communicator](backend::Disembody<Endpoint> disembody){
			// Request comes from the participant itself?
// 			std::cout << "Received disembody: " << disembody.endpoint << " from " << communicator.getRemoteEndpoint() << std::endl;
			if(disembody.endpoint.port() == communicator.getRemoteEndpoint().port()) {
				// send disembody to all neighbours
				try {
					Endpoint remoteEp = communicator.getRemoteEndpoint();
					backend::RoleId disembodiedRoleId = participants.left.at(remoteEp);

					sendToNeighbor(communicator, disembodiedRoleId, backend::Disembody<Endpoint>{ remoteEp });

				} catch(const std::exception& e) {
					std::cerr << "Participant, that is not registered wants to disembody itself." << std::endl;
					std::cerr << e.what() << std::endl;
				}
				return;
			} else {
				// Request comes from someone other
				// Check if he is alive
				// TODO -- disabled at the moment
				std::cerr << "Disembody other clients not yet implemented" << std::endl;
			}
		},
		[this](backend::ServerClose) {
			running = false;
		}
	);

	try {
		communicator.bind(port);
		serverEndpoint = communicator.getLocalEndpoint();
		runningPromise.set_value(true);
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

} // End of namespace cracen2
