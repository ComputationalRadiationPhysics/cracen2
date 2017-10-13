#pragma once

#include <future>
#include <limits>

#include "cracen2/util/Thread.hpp"
#include "cracen2/util/AtomicQueue.hpp"

namespace cracen2 {

namespace util {

class ThreadPool {
	// This variable is set to false, if the ThreadPool is tried to be deconstructed
	volatile bool running;

	// Queue of tasks to do
	AtomicQueue<std::function<void()>> tasks;

	// The thread pool
	std::vector<JoiningThread> threads;

	// the executed function of the spawned threads
	void run() {
		while(running or tasks.size() > 0) {
			// Try to get a task to do. If not successful continue spinning,
			// else execute the task
			auto result = tasks.tryPop(std::chrono::milliseconds(1000));
			if(result) {
				result->operator()();
			}
		}
	}

public:

	ThreadPool(unsigned int threadCount) :
		running(true),
		tasks(std::numeric_limits<std::size_t>::max())
	{
		for(unsigned int i = 0; i < threadCount; i++) {
			threads.emplace_back(&ThreadPool::run, this);
		}
	}

	// The destructor will make all threads joinable, by joining them. This
	// may or may not be wanted. If one thread is stuck, because a task can not
	// be completed. The destructor will not finish.
	~ThreadPool() {
		running = false;
	}

	// This is the method to add a new task to the queue.
	template <class Functor>
	auto async(Functor&& function) {
		using ResultType = decltype(function());

		auto promise = std::make_shared<std::promise<ResultType>>();
		auto future = promise->get_future();

		tasks.push([promise = std::move(promise), function = std::forward<Functor>(function)]() mutable {
			promise->set_value(function());
		});

		// move the future out
		return future;
	}

	template <class Functor>
	void exec(Functor&& function) {
		tasks.push(std::forward<Functor>(function));
	}
};

} // End of namespace util

} // End of namespace cracen2
