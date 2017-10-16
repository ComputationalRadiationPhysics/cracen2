#pragma once

#include "util/AtomicQueue.hpp"
#include "util/Thread.hpp"
#include "CracenClient.hpp"

#include <initializer_list>
#include <numeric>

namespace cracen2 {

namespace detail {

template <class NumericTypes>
NumericTypes sum(std::initializer_list<NumericTypes> values) {
	return std::accumulate(values.begin(), values.end(), 0);
}

} // End of namespace detail

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
	QueueType outputQueues;
	util::AtomicQueue<std::function<void()>> sendActions;

	util::JoiningThread inputThread;
	util::JoiningThread outputThread;

	ClientType client;

	//input and output fifo;

	backend::RoleId roleId;

	template <class T>
	std::function<void(T, typename ClientType::Endpoint)> createVisitorLambda() {
		return [this](T element, Endpoint){
			constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
			auto& queue = std::get<id>(outputQueues);
			queue.push(element);
		};
	}


	void receiver() {

		bool running = true;
		// This is only the workaround for gcc/g++

		auto visitor = ClientType::make_visitor(
			[&running](backend::CracenClose, Endpoint){
				running = false;
			},
			createVisitorLambda<MessageTypeList>()...
		);

		while(client.isRunning() && running) {
			try {
				client.receive(visitor);
			} catch(const std::exception& e) {
				std::cout << "receiver catched exception e: " << e.what() << std::endl;
// 				return;
			}
		}
	}

	void sender() {
		while(client.isRunning()) {
			try {
				sendActions.pop()();
			} catch(const std::exception&) {
				return;
			}
		}
	}

public:

	using Endpoint = typename SocketImplementation::Endpoint;

	Cracen2(Endpoint cracenServerEndpoint, Role role) :
		inputQueues{Role::template InputQueueSize<MessageTypeList>::value...},
		outputQueues{Role::template InputQueueSize<MessageTypeList>::value...},
		sendActions(detail::sum({ Role::template InputQueueSize<MessageTypeList>::value... })),
		client(cracenServerEndpoint, role.roleId, role.roleConnectionGraph),
		roleId(role.roleId)
	{
		inputThread = { &CracenType::receiver, this };
		outputThread = { &Cracen2::sender, this };
	}

	~Cracen2() //= default;
	{
		sendActions.destroy();
		std::vector<int>{
			std::get<
				util::tuple_index<util::AtomicQueue<MessageTypeList>, QueueType>::value
			>(outputQueues).destroy()...
		};
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

	template <class T, class SendPolicy>
	void send(T&& value, SendPolicy sendPolicy) {
		constexpr size_t id = util::tuple_index<
		util::AtomicQueue<
			typename std::remove_reference<T>::type>,
			QueueType
		>::value;
		std::get<id>(inputQueues).push(std::forward<T>(value));
		sendActions.push([this, sendPolicy](){
			auto value = std::get<id>(inputQueues).pop();
			client.send(value, sendPolicy);
		});
	}

	template <class T>
	T receive() {
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;
		return std::get<id>(outputQueues).pop();
	}

	template <class Visitor>
	void receive(Visitor&& visitor) {
		client.receive(std::forward<Visitor>(visitor));
	}

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
