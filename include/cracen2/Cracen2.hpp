#pragma once

#include "util/AtomicQueue.hpp"
#include "util/Thread.hpp"
#include "CracenClient.hpp"

#include <initializer_list>
#include <numeric>

#include <boost/variant.hpp>

namespace cracen2 {

template<class SocketImplementation, class Role, class TagList>
class Cracen2;


/**
 * @brief class for end to end communcication between logical nodes of the communication graph.
 *
 * @tparam SocketImplementation backend for point to point communication
 * @tparam Role Datatype for the mapping to a logical node. Additional information can be attached to the
 * role description, like the input and output queue size for each message type.
 * @tparam MessageTypeList... Each message type, that can be sent or received inside the context
 */

template<class SocketImplementation, class Role, class... MessageTypeList>
class Cracen2<SocketImplementation, Role, std::tuple<MessageTypeList...>> {
public:

	using TagList = std::tuple<backend::CracenClose, MessageTypeList...>;
	using QueueType = std::tuple<util::AtomicQueue<MessageTypeList>...>;
	using ClientType = CracenClient<SocketImplementation, TagList>;
	using CracenType = Cracen2<SocketImplementation, Role, std::tuple<MessageTypeList...>>;
	using RoleEndpointMap = typename ClientType::RoleEndpointMap::value_type;
	using RoleEndpointReadonlyView = typename ClientType::RoleEndpointMap::ReadOnlyView;
	using RoleEndpointView = typename ClientType::RoleEndpointMap::View;

private:

	QueueType inputQueues;

	util::AtomicQueue<
		std::pair<
			std::future<void>,
			boost::variant<std::shared_ptr<MessageTypeList>...>
		>
	> pendingSends;

	util::JoiningThread inputThread;
	util::JoiningThread outputThread;

	ClientType client;

	//input and output fifo;

	backend::RoleId roleId;

	template <class T>
	std::function<void(T, typename ClientType::Endpoint)> createVisitorLambda() {
		return [this](T element, Endpoint) {
			constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
			auto& queue = std::get<id>(inputQueues);
			queue.push(element);
		};
	}


	void receiver() {

		bool running = true;


		std::queue<std::future<void>> pendingReceives;

		auto visitor = ClientType::make_visitor(
			[&running](backend::CracenClose, Endpoint){
				running = false;
			},
			createVisitorLambda<MessageTypeList>()...
		);

		// This is only the workaround for gcc/g++
		for(unsigned int i = 0; i < 100; i++) {
			pendingReceives.push(client.asyncReceive(visitor));
		}

		while(client.isRunning() && running) {
			pendingReceives.front().get();
			pendingReceives.pop();
			pendingReceives.push(client.asyncReceive(visitor));
		}
	}

	void sender() {

		while(client.isRunning()) {

			auto p = pendingSends.tryPop(std::chrono::seconds(1));
			if(p) {
				p->first.get();
			}

		}

	}

public:

	// Forwarding endpoint type information from the socket backend
	using Endpoint = typename SocketImplementation::Endpoint;

	/* @brief Cracen2 constructor
	 * @param cracenServerEndpoint endpoint of the managment server. Upon creation, cracen2 will establish
	 * a connection to the server in order to get information about participants, that enter or leave the context.
	 * @param role The object, that maps this instance to a logical node in the communication graph.
	 */
	Cracen2(Endpoint cracenServerEndpoint, Role role) :
		inputQueues{Role::template InputQueueSize<MessageTypeList>::value...},
		pendingSends(200),
		client(cracenServerEndpoint, role.roleId, role.roleConnectionGraph),
		roleId(role.roleId)
	{
		inputThread = { "Cracen2::ionputThread", &CracenType::receiver, this };
		outputThread = { "Cracen2::outputThread", &Cracen2::sender, this };
	}

	~Cracen2() //= default;
	{
		std::vector<int>{
			std::get<
				util::tuple_index<util::AtomicQueue<MessageTypeList>, QueueType>::value
			>(inputQueues).destroy()...
		};
	}

	/*
	 * Copy constructor got explicitly deleted, because the internal state of cracen, including the in- and outgoing connections is not trivially
	 * copyable.
	 */
	Cracen2(const Cracen2& other) = delete;
	Cracen2& operator=(const Cracen2& other) = delete;

	/*
	 * Move constructor may be implemented at a later point. There is no strong argument against the feasability of move sematics on cracen2.
	 */
	Cracen2(Cracen2&& other) = delete;
	Cracen2& operator=(Cracen2&& other) = delete;


	/*
	 * @brief helper function to make a valid visitor object from lambda functions.
	 *
	 * @param args... Comma seperated list of lambda functions. The Functions shall take a type from MessageList... as value and are used as callback
	 * function, if such a message is received. It is also possible for the visitor to return a value, if and only if all functors share the same
	 * result type.
	 */
// 	template <class... Functors>
// 	static auto make_visitor(Functors&&... args) {
// 		return ClientType::make_visitor(std::forward<Functors>(args)...);
// 	}

	/*
	 * @param value, value to be send
	 * @param sendPolicy functor, that picks all endpoints, to which the value shall be sendet. Cracen comes with the following
	 * send_policies implemented: round_robin, broadcast, and single.
	 */
	template <class T, class SendPolicy>
	void send(T&& value, SendPolicy&& sendPolicy) {

		while(pendingSends.size() > 20) {};

		auto buffer = std::make_shared<std::remove_reference_t<T>>(std::forward<T>(value));
		auto futures = client.asyncSend(*buffer, std::forward<SendPolicy>(sendPolicy));

		for(auto& f : futures) {
			pendingSends.push(
				std::make_pair(
					std::move(f),
					boost::variant<std::shared_ptr<MessageTypeList>...>(buffer)
				)
			);
		}
	}


	/*
	 * @result returns the number of messages of type T in message box.
	 */
	template <class T>
	std::size_t count() {
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
		return std::get<id>(inputQueues).size();
	}

	/*
	 * @brief blocking receive operation
	 * @result returns a value of type T, that has been received on an edge.
	 */
	template <class T>
	T receive() {
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
		return std::get<id>(inputQueues).pop();
	}

// 	template <class Visitor>
// 	void receive(Visitor&& visitor) {
// 		client.receive(std::forward<Visitor>(visitor));
// 	}

	/*
	 * @brief function to return the mapping from logical nodes to physical endpoints. This data can be used to enforce specific predicates on the context. E.g. every logical node should be incorperated by at least one physical node.
	 */
	decltype(client.getRoleEndpointMapReadOnlyView()) getRoleEndpointMapReadOnlyView() {
		return client.getRoleEndpointMapReadOnlyView();
	}

	/*
	 * @brief same as getRoleEndpointMapReadOnlyView(), but will block until the predicate is satisfied.
	 */
	template <class Predicate>
	decltype(client.getRoleEndpointMapReadOnlyView()) getRoleEndpointMapReadOnlyView(Predicate&& predicate) {
		return client.getRoleEndpointMapReadOnlyView(std::forward<Predicate>(predicate));
	}

	/*
	 * @brief function to print debug information to std::cout.
	 */
	void printStatus() {
		client.printStatus();
	}

	/*
	 *  @brief release the cracen. Finalize the context and safely close all connections.
	 */
	void release() {
		client.loopback(backend::CracenClose());
		client.stop();
		while(client.isRunning()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

}; // End of class cracen2

} // End of namespace cracen2
