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
class Thread {
private:
	std::thread thread;
	bool running;
public:
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
		if(running) {
			detail::ThreadDeletionAction<deletionPolicy>()(thread);
		}
		thread = std::move(other.thread);
		running = other.running;
		other.running = false;
		return *this;
	}

	Thread(const Thread&) = delete;

	template< class Function, class... Args >
	Thread(std::string name, Function function, Args&&... args) :
		thread(function, std::forward<Args>(args)...),
		running(true)
	{
		pthread_setname_np(thread.native_handle(), name.substr(0,15).c_str());
	}

	~Thread() {
		if(running) {
			detail::ThreadDeletionAction<deletionPolicy>()(thread);
		}
	}
};

extern template class Thread<ThreadDeletionPolicy::join>;
extern template class Thread<ThreadDeletionPolicy::detatch>;

using JoiningThread = Thread<ThreadDeletionPolicy::join>;
using DetatchingThread = Thread<ThreadDeletionPolicy::detatch>;

} // End of namespace Util

} // End of namespace cracen2
