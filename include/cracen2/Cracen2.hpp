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

	using Endpoint = typename SocketImplementation::Endpoint;

	Cracen2(Endpoint cracenServerEndpoint, Role role) :
		inputQueues{Role::template InputQueueSize<MessageTypeList>::value...},
		pendingSends(200),
		client(cracenServerEndpoint, role.roleId, role.roleConnectionGraph),
		roleId(role.roleId)
	{
		inputThread = { &CracenType::receiver, this };
		outputThread = { &Cracen2::sender, this };
	}

	~Cracen2() //= default;
	{
		std::vector<int>{
			std::get<
				util::tuple_index<util::AtomicQueue<MessageTypeList>, QueueType>::value
			>(inputQueues).destroy()...
		};
	}

	Cracen2(const Cracen2& other) = delete;
	Cracen2& operator=(const Cracen2& other) = delete;

	Cracen2(Cracen2&& other) = delete;
	Cracen2& operator=(Cracen2&& other) = delete;

	template <class... Functors>
	static auto make_visitor(Functors&&... args) {
		return ClientType::make_visitor(std::forward<Functors>(args)...);
	}

	#warning add perfect forwarding for send policy
	template <class T, class SendPolicy>
	void send(T&& value, SendPolicy sendPolicy) {

		auto buffer = std::make_shared<std::remove_reference_t<T>>(std::forward<T>(value));
		auto futures = client.asyncSend(*buffer, sendPolicy);

		for(auto& f : futures) {
			pendingSends.push(
				std::make_pair(
					std::move(f),
					boost::variant<std::shared_ptr<MessageTypeList>...>(buffer)
				)
			);
		}
	}

	template <class T>
	std::size_t count() {
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
		return std::get<id>(inputQueues).size();
	}

	template <class T>
	T receive() {
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
		return std::get<id>(inputQueues).pop();
	}

// 	template <class Visitor>
// 	void receive(Visitor&& visitor) {
// 		client.receive(std::forward<Visitor>(visitor));
// 	}

	decltype(client.getRoleEndpointMapReadOnlyView()) getRoleEndpointMapReadOnlyView() {
		return client.getRoleEndpointMapReadOnlyView();
	}

	template <class Predicate>
	decltype(client.getRoleEndpointMapReadOnlyView()) getRoleEndpointMapReadOnlyView(Predicate&& predicate) {
		return client.getRoleEndpointMapReadOnlyView(std::forward<Predicate>(predicate));
	}

	void printStatus() {
		client.printStatus();
	}

	void release() {
		client.loopback(backend::CracenClose());
		client.stop();
		while(client.isRunning()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

}; // End of class cracen2

} // End of namespace cracen2
