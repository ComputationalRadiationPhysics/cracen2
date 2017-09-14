#include "cracen2/util/Signal.hpp"

std::function<void(int)> cracen2::util::SignalHandler::deleter;

void cracen2::util::SignalHandler::rawSignalHandler(int signum) {
	if(deleter) deleter(signum);
}
