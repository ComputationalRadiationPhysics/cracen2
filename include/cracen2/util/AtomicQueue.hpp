#pragma once

#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <boost/optional.hpp>

namespace cracen2 {

namespace util {

template <class Type>
class AtomicQueue {
private:
	std::mutex mutex;
	std::condition_variable poped, pushed;

	std::queue<Type> data;
	const std::size_t maxSize;
	bool active;

	bool notFull() {
		if(!active) {
			throw std::runtime_error("AtomicQueue is in inactive state");
		}
		return data.size() < maxSize;
	}

	bool isFilled() {
		if(!active) {
			throw std::runtime_error("AtomicQueue is in inactive state");
		}
		return data.size() > 0;
	}

public:
	using value_type = Type;

	AtomicQueue(std::size_t maxSize) :
		maxSize(maxSize),
		active(true)
	{}

	~AtomicQueue() = default;

	AtomicQueue(const AtomicQueue& other) = delete;
	AtomicQueue& operator=(const AtomicQueue& other) = delete;

	AtomicQueue(AtomicQueue&& other) = delete;
	AtomicQueue& operator=(AtomicQueue&& other) = delete;

	void push(const Type& value) {
		std::unique_lock<std::mutex> lock(mutex);
		poped.wait(lock, [this]() -> bool {return notFull();});
		data.push(value);
		pushed.notify_one();
	}

	void push(Type&& value) {
		std::unique_lock<std::mutex> lock(mutex);
		poped.wait(lock,[this]() -> bool { return notFull(); } );
		data.push(std::forward<Type>(value));
		pushed.notify_one();
	}

	Type pop() {
		std::unique_lock<std::mutex> lock(mutex);
		pushed.wait(lock, [this]() -> bool {return isFilled();});
		Type result = std::move(data.front());
		data.pop();
		poped.notify_one();
		return result;
	}

	boost::optional<Type> tryPop(std::chrono::milliseconds timeout) {
		std::unique_lock<std::mutex> lock(mutex);
		if(pushed.wait_for(lock, timeout, [this]() -> bool {return isFilled();})) {
			Type result = std::move(data.front());
			data.pop();
			poped.notify_one();
			return result;
		} else {
			return boost::none;
		}
	}

	size_t size() {
		return data.size();
	};

	int destroy() {
		{
			std::unique_lock<std::mutex> lock(mutex);
			active = false;
		}
		pushed.notify_all();
		poped.notify_all();
		return 0;
	}
};

}

}

