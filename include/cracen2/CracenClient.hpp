#pragma once

#include <map>
#include <vector>
#include <sstream>

#include "cracen2/network/Communicator.hpp"
#include "cracen2/backend/Messages.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/CoarseGrainedLocked.hpp"

namespace cracen2 {

template <class SocketImplementation, class DataTagList>
class CracenClient {
public:

	using TagList = backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using ServerCommunicator = network::Communicator<SocketImplementation, TagList>;
	using DataCommunicator = network::Communicator<SocketImplementation, DataTagList>;

	using Endpoint = typename ServerCommunicator::Endpoint;
	using Port = typename ServerCommunicator::Port;
	using ServerVisitor = typename ServerCommunicator::Visitor;
	using DataVisitor = typename DataCommunicator::Visitor;

	using Edge = std::pair<backend::RoleId, backend::RoleId>;
	using RoleCommunicatorMap = util::CoarseGrainedLocked<
		std::map<
			backend::RoleId,
			std::vector<
				std::unique_ptr<DataCommunicator>
			>
		>
	>;

private:
	static constexpr Port minPort = 39500;
	static constexpr Port intervalPort = 3500; // Amount of ports, that client tries to bind to

	const backend::RoleId roleId;
	ServerCommunicator serverCommunicator;
	DataCommunicator dataCommunicator;


	RoleCommunicatorMap roleCommunicatorMap;

	util::JoiningThread managmentThread;
	bool running;

	void alive() {

		ServerVisitor visitor(
			[&](backend::Disembody<Endpoint> disembody){
				if(disembody.endpoint == dataCommunicator.getLocalEndpoint()) {
					// Disembody Ack
					running = false;
				} else {
					// Someone else disembodied. Remove his endpoint from role list.
					auto roleCommunicatorView = roleCommunicatorMap.getView();
					for(auto& roleCommVecPair : roleCommunicatorView->get()) {
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
				// std::cout << "Receive embody" << std::endl;
				try {
					DataCommunicator* com = new DataCommunicator;
					com->connect(embody.endpoint);
					auto roleCommunicatorView = roleCommunicatorMap.getView();
					auto& map = roleCommunicatorView->get();
					map[embody.roleId].push_back(std::unique_ptr<DataCommunicator>(com));
				} catch(const std::exception& e) {
					std::cerr << "Could not connect to " << embody.endpoint << ". Ignoring embody(" << embody.roleId << ")"<< std::endl;
				}

			},
			[&](backend::Announce<Endpoint> embody){
				// Embody someone
				// std::cout << "Receive announce" << std::endl;
				if(network::IsDatagramSocket<SocketImplementation>::value) {
					try {
						DataCommunicator* com = new DataCommunicator;
						com->connect(embody.endpoint);
						auto roleCommunicatorView = roleCommunicatorMap.getView();
						auto& map = roleCommunicatorView->get();
						map[embody.roleId].push_back(std::unique_ptr<DataCommunicator>(com));
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
		roleId(roleId),
		running(true)
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

		ServerVisitor contextCreationVisitor(
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
		auto roleCommunicatorView = roleCommunicatorMap.getReadOnlyView();
		const auto& map = roleCommunicatorView->get();
		sendPolicy.run(std::forward<T>(message), map);
	}

	template <class T>
	void loopback(T&& message) {
		dataCommunicator.connect(dataCommunicator.getLocalEndpoint());
		dataCommunicator.send(std::forward<T>(message));
	}

	template<class T>
	T receive() {
		return dataCommunicator.template receive<T>();
	}

	void receive(DataVisitor& visitor) {
		dataCommunicator.receive(std::forward<DataVisitor>(visitor));
	}

	backend::RoleId getRoleId() const {
		return roleId;
	}

	void stop() {
		serverCommunicator.send(backend::Disembody<Endpoint>{ dataCommunicator.getLocalEndpoint() });
	};

	bool isRunning() {
		return running;
	}

	decltype(roleCommunicatorMap.getReadOnlyView()) getRoleCommunicatorMapReadOnlyView() {
		return roleCommunicatorMap.getReadOnlyView();
	}

	template <class Predicate>
	decltype(roleCommunicatorMap.getReadOnlyView()) getRoleCommunicatorMapReadOnlyView(Predicate&& predicate) {
		return roleCommunicatorMap.getReadOnlyView(std::forward<Predicate>(predicate));
	}

	void printStatus() const {
		std::stringstream status;
		status
			<< "Status for client:\n"
			<< "	roleId: " << roleId << "\n"
			<< "	serverEndpoint:" << serverCommunicator.getLocalEndpoint() << "\n"
			<< "	dataEndpoint:" << dataCommunicator.getLocalEndpoint() <<"\n"
			<< "	connections: [\n";
		for(const auto& connection : roleCommunicatorMap.getReadOnlyView()->get()) {
			for(const auto& comm : connection.second) {
				status << "		role(" << connection.first << ") -> " << comm->getRemoteEndpoint() << "\n";
			}
		}
		status << "	]\n";
		std::cout << status.rdbuf();
	}
}; // End of class CracenClient

} // End of namespace cracen2
