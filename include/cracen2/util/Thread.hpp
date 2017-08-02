#pragma once

#include <thread>

namespace cracen2 {

namespace util {

	enum class ThreadDeletionPolicy {
		join,
		detatch
	};

	namespace detail {
		template <ThreadDeletionPolicy>
		struct ThreadDeletionAction;

		template <>
		struct ThreadDeletionAction<ThreadDeletionPolicy::join> {
			void operator()(std::thread& thread) {
					thread.join();
			}
		};

		template <>
		struct ThreadDeletionAction<ThreadDeletionPolicy::detatch> {
			void operator()(std::thread& thread) {
				thread.detach();
			}
		};

	}

	template <ThreadDeletionPolicy deletionPolicy>
	struct Thread {
		std::thread thread;
		bool running;

		Thread() :
			running(false)
		{};

		Thread(Thread&& other) :
			thread(std::move(other.thread)),
			running(true)
		{
			other.running = false;
		}

		Thread& operator=(Thread&& other) {
			thread = std::move(other.thread);
			running = other.running;
			other.running = false;
			return *this;
		}

		Thread(const Thread&) = delete;

		template< class Function, class... Args >
		Thread(Function function, Args... args) :
			thread(function, args...),
			running(true)
		{}

		~Thread() {
			if(running) {
				detail::ThreadDeletionAction<deletionPolicy>()(thread);
			}
		}
	};

	extern template struct Thread<ThreadDeletionPolicy::join>;
	extern template struct Thread<ThreadDeletionPolicy::detatch>;

	using JoiningThread = Thread<ThreadDeletionPolicy::join>;
	using DetatchingThread = Thread<ThreadDeletionPolicy::detatch>;

} // End of namespace Util

} // End of namespace cracen2
