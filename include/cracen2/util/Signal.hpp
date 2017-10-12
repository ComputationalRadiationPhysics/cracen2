#pragma once

#include <cstdint>
#include <limits>
#include <functional>
#include <csignal>

#include "cracen2/util/Foreach.hpp"

namespace cracen2 {

namespace util {

class SignalHandler {
private:

	static std::function<void(int)> deleter;

	static void rawSignalHandler(int signum);
public:

	template <class Deleter, class... Args>
	static void handle(Deleter&& del, Args... signals)
	{
		deleter = std::forward<Deleter>(del);
		cracen2::util::foreach(
			[](int sig) { signal(sig, rawSignalHandler); },
			signals...
		);
	}

	template <class Deleter>
	static void handleAll(Deleter&& del) {
		handle(
			std::forward<Deleter>(del),
			SIGABRT,
		 	SIGFPE,
		 	SIGILL,
		 	SIGINT,
		 	SIGSEGV,
		 	SIGTERM
		);
	}

};

} // End of namespace util

} // End of namespace cracen2
