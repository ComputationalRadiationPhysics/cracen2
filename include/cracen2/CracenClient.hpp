#pragma once

#include <map>
#include <vector>
#include <sstream>

#include "cracen2/network/Communicator.hpp"
#include "cracen2/backend/Messages.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/CoarseGrainedLocked.hpp"

namespace cracen2 {


/**
 * @brief Class to communicate between logical nodes, without the use of input and outputqueues. All calls to the underlying communication backend
 * happen directly on the corresponding send or receive call on CracenClient.
 * @tparam SocketImplementation backend socket implementation
 * @tparam DataTagList std::tuple containing all message types that can be sent or received inside the context.
 */
template <class SocketImplementation, class DataTagList>
class CracenClient {
public:

	using TagList = backend::ServerTagList<typename SocketImplementation::Endpoint>;
	using ServerCommunicator = network::Communicator<SocketImplementation, TagList>;
	using DataCommunicator = network::Communicator<SocketImplementation, DataTagList>;

	using Endpoint = typename ServerCommunicator::Endpoint;

	using Edge = std::pair<backend::RoleId, backend::RoleId>;
	using RoleEndpointMap = util::CoarseGrainedLocked<
		std::map<
			backend::RoleId,
			std::vector<Endpoint>
		>
	>;

private:

	const backend::RoleId roleId;
	Endpoint serverEndpoint;
	ServerCommunicator serverCommunicator;

	DataCommunicator dataCommunicator;

	RoleEndpointMap roleEndpointMap;

	util::JoiningThread managmentThread;

	bool running;

	void alive();

public:

	/*
	 * Constructor
	 * @param serverEndpoint physical endpoint of the server. Upon creation a connection the the server will be established to get information about
	 * participants that enter or leave the context.
	 * @param roleId The logical id of this node
	 * @param roleGraph the communication graph.
	 */
	template <class RoleGraphContainerType>
	CracenClient(Endpoint serverEndpoint, backend::RoleId roleId, const RoleGraphContainerType& roleGraph);

	/*
	 * @brief helper function to make a valid visitor object from lambda functions.
	 *
	 * @param args... Comma seperated list of lambda functions. The Functions shall take a type from MessageList... as value and are used as callback
	 * function, if such a message is received. It is also possible for the visitor to return a value, if and only if all functors share the same
	 * result type.
	 */
	template <class... Functors>
	static auto make_visitor(Functors&&... args);



	/*
	 * blocking send operation
	 * @param value, value to be send
	 * @param sendPolicy functor, that picks all endpoints, to which the value shall be sendet. Cracen comes with the following
	 * send_policies implemented: round_robin, broadcast, and single.
	 */
	template <class T, class SendPolicy>
	void send(T&& message, SendPolicy sendPolicy);

	/*
	 * Same as send, but using asynchrous communication
	 */
	template <class T, class SendPolicy>
	std::vector<std::future<void>> asyncSend(const T& message, SendPolicy sendPolicy);

	/*
	 * blocking receive. Since there are no message queses, the type of the message has to be guessed right. If the type of the received message does not equal T, a exception will be thrown. This exception can be cought, but the message will be lost. If the type of the received message is not known, this function should not be called.
	 */
	template<class T>
	T receive();

	/*
	 * asynchrounus version of receive()
	 */
	template<class T>
	std::future<T> asyncReceive();

	/*
	 * Blocking receive operation.
	 * @visitor function object that will be used as callback function after the message is received. The message will be passed as typed argument to
	 * the callback. If the type of the message is not known beforehand, there should be an overload for each possible message type.
	 *
	 */
	template <class DataVisitor>
	auto receive(DataVisitor&& visitor);

	/*
	 * asynchrounus version of receive(visitor)
	 */
	template <class DataVisitor>
	auto asyncReceive(DataVisitor&& visitor);

	/*
	 * @result virtual address of this node
	 */
	backend::RoleId getRoleId() const;

	/*
	 * send a message to itself
	 */
	template <class Message>
	void loopback(Message message);

	/*
	 * Stop sending and receiving messages. The underlying workerthreads will execute the remaining work and then terminate.
	 */
	void stop();

	/*
	 * Check if in state running. (sending and receiving possible)
	 */
	bool isRunning();

	/*
	 * @brief function to return the mapping from logical nodes to physical endpoints. This data can be used to enforce specific predicates on the context. E.g. every logical node should be incorperated by at least one physical node.
	 */
	auto getRoleEndpointMapReadOnlyView();

	/*
	 * @brief same as getRoleEndpointMapReadOnlyView(), but will block until the predicate is satisfied.
	 */
	template <class Predicate>
	auto getRoleEndpointMapReadOnlyView(Predicate&& predicate);

	/*
	 * @brief function to print debug information to std::cout.
	 */
	void printStatus() const;

}; // End of class CracenClient

template <class SocketImplementation, class DataTagList>
void CracenClient<SocketImplementation, DataTagList>::alive() {

	auto visitor = ServerCommunicator::make_visitor(
		[&](backend::Disembody<Endpoint> disembody, Endpoint from){
			if(disembody.endpoint == dataCommunicator.getLocalEndpoint()) {
				// Disembody Ack
				std::cout << "Client received disembody" << std::endl;
				running = false;
			} else {
				// Someone else disembodied. Remove his endpoint from role list.
				auto roleCommunicatorView = roleEndpointMap.getView();
				for(auto& roleCommVecPair : roleCommunicatorView->get()) {
					auto& commVec = roleCommVecPair.second;
					decltype(commVec.begin()) position;
					for(position = commVec.begin(); position != commVec.end(); position++) {
						if(from == disembody.endpoint) break;
					}
					if(position != commVec.end()) {
						commVec.erase(position);
					}
				}
			}
		},
		[&](backend::Embody<Endpoint> embody, Endpoint){
			// Embody someone
			// std::cout << "Receive embody" << embody.roleId << " " << embody.endpoint<< std::endl;
			try {
				auto roleCommunicatorView = roleEndpointMap.getView();
				auto& map = roleCommunicatorView->get();
				map[embody.roleId].push_back(embody.endpoint);
			} catch(const std::exception& e) {
				std::cerr << "Could not connect to " << embody.endpoint << ". Ignoring embody(" << embody.roleId << ")"<< std::endl;
			}

		},
		[&](backend::Announce<Endpoint> announce, Endpoint){
			// Embody someone
			// std::cout << "Receive announce" << std::endl;
			try {
				auto roleCommunicatorView = roleEndpointMap.getView();
				auto& map = roleCommunicatorView->get();
				map[announce.roleId].push_back(announce.endpoint);
			} catch(const std::exception& e) {
				std::cerr << "Could not connect to " << announce.endpoint << ". Ignoring embody(" << announce.roleId << ")"<< std::endl;
			}
		}
	);

	while(running) {
		serverCommunicator.receive(visitor);
	}
}

template <class SocketImplementation, class DataTagList>
template <class RoleGraphContainerType>
CracenClient<SocketImplementation, DataTagList>::CracenClient(Endpoint serverEndpoint, backend::RoleId roleId, const RoleGraphContainerType& roleGraph) :
	roleId(roleId),
	serverEndpoint(serverEndpoint),
	running(true)
{
	dataCommunicator.bind();
	serverCommunicator.bind();
	std::cout << "send register to " << serverEndpoint << std::endl;
	serverCommunicator.sendTo(backend::Register(), serverEndpoint);

	bool contextReady = false;
	unsigned int edges = 0;

	auto contextCreationVisitor = ServerCommunicator::make_visitor(
		[this, &roleGraph, serverEndpoint](backend::RoleGraphRequest, Endpoint){
			// Request from server to send role graph
			for(const auto edge : roleGraph) {
				// send connections one by one
				serverCommunicator.sendTo(backend::AddRoleConnection { edge.first, edge.second }, serverEndpoint);
			}
			serverCommunicator.sendTo(backend::RolesComplete(), serverEndpoint);
		},
		[&edges](backend::AddRoleConnection, Endpoint){ ++edges; },
		[&contextReady](backend::RolesComplete, Endpoint){
			contextReady = true;
		}
	);

	do {
		std::cout << "Wait for answer..." << std::endl;
		serverCommunicator.receive(contextCreationVisitor);
	} while(!contextReady);
	std::cout << "Send Embody " << roleId << " " << dataCommunicator.getLocalEndpoint() << std::endl;
	serverCommunicator.sendTo(backend::Embody<Endpoint>{ dataCommunicator.getLocalEndpoint(), roleId }, serverEndpoint);

	managmentThread = util::JoiningThread("CracenClient::managmentThread", &CracenClient::alive, this);
}

template <class SocketImplementation, class DataTagList>
template <class... Functors>
auto CracenClient<SocketImplementation, DataTagList>::make_visitor(Functors&&... args) {
	return DataCommunicator::make_visitor(std::forward<Functors>(args)...);
}

template <class SocketImplementation, class DataTagList>
template <class T, class SendPolicy>
void CracenClient<SocketImplementation, DataTagList>::send(T&& message, SendPolicy sendPolicy) {
	auto roleEndpointView = roleEndpointMap.getReadOnlyView();
	const auto& map = roleEndpointView->get();
	auto eps = sendPolicy.run(map);
	for(auto& ep : eps) {
		dataCommunicator.sendTo(message, ep);
	}
}

template <class SocketImplementation, class DataTagList>
template <class T, class SendPolicy>
std::vector<std::future<void>> CracenClient<SocketImplementation, DataTagList>::asyncSend(const T& message, SendPolicy sendPolicy) {
	std::vector<std::future<void>> result;
	auto roleEndpointView = roleEndpointMap.getReadOnlyView();
	const auto& map = roleEndpointView->get();
	auto eps = sendPolicy.run(map);
	for(auto& ep : eps) {
		result.emplace_back(dataCommunicator.asyncSendTo(message, ep));
	}

	return result;
}

template <class SocketImplementation, class DataTagList>
template<class T>
T CracenClient<SocketImplementation, DataTagList>::receive() {
	return dataCommunicator.template receive<T>();
}

template <class SocketImplementation, class DataTagList>
template<class T>
std::future<T> CracenClient<SocketImplementation, DataTagList>::asyncReceive() {
	return dataCommunicator.template asyncReceive<T>();
}

template <class SocketImplementation, class DataTagList>
template <class DataVisitor>
auto CracenClient<SocketImplementation, DataTagList>::receive(DataVisitor&& visitor) {
	return dataCommunicator.receive(std::forward<DataVisitor>(visitor));
}

template <class SocketImplementation, class DataTagList>
template <class DataVisitor>
auto CracenClient<SocketImplementation, DataTagList>::asyncReceive(DataVisitor&& visitor) {
	return dataCommunicator.asyncReceive(std::forward<DataVisitor>(visitor));
}

template <class SocketImplementation, class DataTagList>
backend::RoleId CracenClient<SocketImplementation, DataTagList>::getRoleId() const {
	return roleId;
}

template <class SocketImplementation, class DataTagList>
template <class Message>
void CracenClient<SocketImplementation, DataTagList>::loopback(Message message) {
	dataCommunicator.sendTo(std::forward<Message>(message), dataCommunicator.getLocalEndpoint());
}

template <class SocketImplementation, class DataTagList>
void CracenClient<SocketImplementation, DataTagList>::stop() {
	serverCommunicator.sendTo(backend::Disembody<Endpoint>{ dataCommunicator.getLocalEndpoint() }, serverEndpoint);
	managmentThread = util::JoiningThread();
	serverCommunicator.close();
	dataCommunicator.close();
};

template <class SocketImplementation, class DataTagList>
bool CracenClient<SocketImplementation, DataTagList>::isRunning() {
	return running;
}

template <class SocketImplementation, class DataTagList>
auto CracenClient<SocketImplementation, DataTagList>::getRoleEndpointMapReadOnlyView() {
	return roleEndpointMap.getReadOnlyView();
}

template <class SocketImplementation, class DataTagList>
template <class Predicate>
auto CracenClient<SocketImplementation, DataTagList>::getRoleEndpointMapReadOnlyView(Predicate&& predicate) {
	return roleEndpointMap.getReadOnlyView(std::forward<Predicate>(predicate));
}

template <class SocketImplementation, class DataTagList>
void CracenClient<SocketImplementation, DataTagList>::printStatus() const {
	std::stringstream status;
	status
		<< "Status for client:\n"
		<< "	roleId: " << roleId << "\n"
		<< "	serverEndpoint:" << serverCommunicator.getLocalEndpoint() << "\n"
		<< "	dataEndpoint:" << dataCommunicator.getLocalEndpoint() <<"\n"
		<< "	connections: [\n";
	for(const auto& connection : roleEndpointMap.getReadOnlyView()->get()) {
		for(const auto& ep : connection.second) {
			status << "		role(" << connection.first << ") -> " << ep << "\n";
		}
	}
	status << "	]\n";
	std::cout << status.rdbuf();
}

} // End of namespace cracen2
