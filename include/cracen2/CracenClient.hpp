#pragma once

#include <map>
#include <vector>

#include "cracen2/network/Communicator.hpp"
#include "cracen2/backend/Messages.hpp"
#include "cracen2/util/Thread.hpp"

namespace cracen2 {

template <class SocketImplementation, class DataTagList>
class CracenClient {
private:

	using TagList = backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using ServerCommunicator = network::Communicator<SocketImplementation, TagList>;
	using DataCommunicator = network::Communicator<SocketImplementation, DataTagList>;

	using Endpoint = typename ServerCommunicator::Endpoint;
	using Port = typename ServerCommunicator::Port;
	using Visitor = typename ServerCommunicator::Visitor;

	using Edge = std::pair<backend::RoleId, backend::RoleId>;

	static constexpr Port minPort = 39500;
	static constexpr Port intervalPort = 3500; // Amount of ports, that client tries to bind to

	const backend::RoleId roleId;
	ServerCommunicator serverCommunicator;
	DataCommunicator dataCommunicator;

	using RoleCommunicatorMap = std::map<backend::RoleId, std::vector<std::unique_ptr<DataCommunicator>>>;
	RoleCommunicatorMap roleCommunicatorMap;

	util::JoiningThread managmentThread;

	void alive() {
		bool running = true;

		Visitor visitor(
			[&](backend::Disembody<Endpoint> disembody){
				if(disembody.endpoint == dataCommunicator.getLocalEndpoint()) {
					// Disembody Ack
					running = false;
				} else {
					// Someone else disembodied. Remove his endpoint from role list.
					for(auto& roleCommVecPair : roleCommunicatorMap) {
						auto& commVec = roleCommVecPair.second;
						decltype(commVec.begin()) position;
						for(position = commVec.begin(); position != commVec.end(); position++) {
							if((*position)->getRemoteEndpoint() == disembody.endpoint) break;
						}
						if(position != commVec.end()) {
							commVec.erase(position);
						}
					}
				}
			},
			[&](backend::Embody<Endpoint> embody){
				// Embody someone
				try {
					DataCommunicator* com = new DataCommunicator;
					com->connect(embody.endpoint);
					roleCommunicatorMap[embody.roleId].push_back(std::unique_ptr<DataCommunicator>(com));
				} catch(const std::exception& e) {
					std::cerr << "Could not connect to " << embody.endpoint << ". Ignoring embody(" << embody.roleId << ")"<< std::endl;
				}

			},
			[&](backend::Announce<Endpoint> embody){
				// Embody someone
				if(network::IsDatagramSocket<SocketImplementation>::value) {
					try {
						DataCommunicator* com = new DataCommunicator;
						com->connect(embody.endpoint);
						roleCommunicatorMap[embody.roleId].push_back(std::unique_ptr<DataCommunicator>(com));
					} catch(const std::exception& e) {
						std::cerr << "Could not connect to " << embody.endpoint << ". Ignoring embody(" << embody.roleId << ")"<< std::endl;
					}
				} else {
					//TODO: wait for incoming connect and register that connection to roleCommunicatorMap
				}
			}
		);

		while(running) {
			serverCommunicator.receive(visitor);
		}
	}

public:

	template <class RoleGraphContainerType>
	CracenClient(Endpoint serverEndpoint, backend::RoleId roleId, const RoleGraphContainerType& roleGraph) :
		roleId(roleId)
	{
		for(Port port = minPort; port < minPort + intervalPort; port++) {
			try {
				dataCommunicator.bind(port);
				break;
			} catch(const std::exception&) {

			}
		}
		if(!dataCommunicator.isOpen()) {
			throw std::runtime_error("Could not bind the dataCommunicator to a port");
		}
		serverCommunicator.connect(serverEndpoint);
		serverCommunicator.send(backend::Register());

		bool contextReady = false;
		unsigned int edges = 0;

		Visitor contextCreationVisitor(
			[this, &roleGraph](backend::RoleGraphRequest){
				// Request from server to send role graph
				for(const auto edge : roleGraph) {
					// send connections one by one
					serverCommunicator.send(backend::AddRoleConnection { edge.first, edge.second });
				}
				serverCommunicator.send(backend::RolesComplete());
			},
			[this, roleId, &edges](backend::AddRoleConnection){ ++edges; },
			[&contextReady, roleId, &edges, &roleGraph](backend::RolesComplete){
				contextReady = true;
			}
		);

		do {
			serverCommunicator.receive(contextCreationVisitor);
		} while(!contextReady);

		serverCommunicator.send(backend::Embody<Endpoint>{ dataCommunicator.getLocalEndpoint(), roleId });

		managmentThread = util::JoiningThread(&CracenClient::alive, this);

	}

	template <class T, class SendPolicy>
	void send(T&& message, SendPolicy sendPolicy) {
		sendPolicy.run(std::forward<T>(message), roleCommunicatorMap);
	}

	template<class T>
	T receive() {
		return dataCommunicator.template receive<T>();
	}

	void receive(Visitor&& visitor) {
		dataCommunicator.receive(std::forward<Visitor>(visitor));
	}

	backend::RoleId getRoleId() const {
		return roleId;
	}

	void stop() {
		serverCommunicator.send(backend::Disembody<Endpoint>{ dataCommunicator.getLocalEndpoint() });
	};


	void printStatus() const {
		std::stringstream status;
		status
			<< "Status for client:\n"
			<< "	roleId: " << roleId << "\n"
			<< "	serverEndpoint:" << serverCommunicator.getLocalEndpoint() << "\n"
			<< "	dataEndpoint:" << dataCommunicator.getLocalEndpoint() <<"\n"
			<< "	connections: [\n";
		for(const auto& connection : roleCommunicatorMap) {
			for(const auto& comm : connection.second) {
				status << "		role(" << connection.first << ") -> " << comm->getRemoteEndpoint() << "\n";
			}
		}
		status << "	]\n";
		std::cout << status.rdbuf();
	}
}; // End of class CracenClient

} // End of namespace cracen2
