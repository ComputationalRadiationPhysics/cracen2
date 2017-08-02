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

	using GraphConnectionType = boost::bimap<
		boost::bimaps::multiset_of<backend::RoleId>,
		boost::bimaps::multiset_of<backend::RoleId>
	>;

	struct Participant {
		backend::RoleId role;
		typename SocketImplementation::Endpoint endpoint;
	};

	enum class State {
		ContextUninitialised,
		ContextInizialising,
		ContextInitialised
	};

	using TagList = backend::ServerTagList;
	using Communicator = network::Communicator<SocketImplementation, TagList>;
	using Endpoint = typename Communicator::Endpoint;
	using Port = typename Communicator::Port;
	using Visitor = typename Communicator::Visitor;

private:

	State state;
	GraphConnectionType roleGraphConnections;

	std::map<Endpoint, Participant> participants;

	std::promise<bool> runningPromise;
	std::atomic<bool> running;
	util::JoiningThread serverThread;
	void run(Port port);

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
	Visitor visitor(
		[this, &communicator](backend::Register reg){

		},
		[this, &communicator](backend::Register reg){
			switch(state) {
				case State::ContextUninitialised:
					// First client is connecting
					std::cout << "Initialising Context..." << std::endl;
					state = State::ContextInizialising;
					communicator.send(backend::RoleGraphRequest());
				break;
				case State::ContextInizialising:
					// Another client is already initialising the role graph
					// add Message back in message queue, since we can't deal with it at the moment

				break;
				case State::ContextInitialised:
					communicator.send(backend::RolesComplete());
				break;
			}
		},
		[this, &communicator](backend::AddRoleConnection addRoleConnection){
			roleGraphConnections.left.insert(std::make_pair(addRoleConnection.from, addRoleConnection.to));
			communicator.send(addRoleConnection); // Reply the same package as ACK
		},
		[this, &communicator](backend::RolesComplete rolesComplete){
			std::cout << "Initialised context" << std::endl;
			for(const auto& edge : roleGraphConnections.left) {
				std::cout << edge.first << "->" << edge.second << std::endl;
			}
			state = State::ContextInitialised;
			communicator.send(rolesComplete);
		},
		[this, &communicator](backend::Embody embody){

		},
		[this, &communicator](backend::Disembody disembody){
			// Request comes from the participant itself
			// send disembody to all neighbours


			// Request comes from someone other
			// Check if he is alive
		}
	);

	try {
		communicator.bind(port);
		runningPromise.set_value(true);
		communicator.accept();
	} catch(const std::exception& e) {
		runningPromise.set_value(false);
		std::cerr << e.what() << std::endl;;
	}


	try {
		while(true) {
			communicator.receive(visitor);
		}
	} catch(const std::exception& e) {
			std::cout << "Closing connection." << std::endl;
	}
}

template <class SocketImplementation>
void CracenServer<SocketImplementation>::stop() {
	running = false;
}

} // End of namespace cracen2
