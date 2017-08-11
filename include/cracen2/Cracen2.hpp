#pragma once

#include "util/AtomicQueue.hpp"
#include "util/Thread.hpp"

namespace cracen2 {

template<class Role>
class Cracen2 {
private:
	//input and output fifo;
	util::AtomicQueue<typename Role::Input> inputQueue;
	util::AtomicQueue<typename Role::Output> outputQueue;
	Role role;

	//input and output threads
	util::JoiningThread inputThread;
	util::JoiningThread kernelThread;
	util::JoiningThread outputThread;

public:

	Cracen2();
	~Cracen2();

	Cracen2(const Cracen2& other) = delete;
	Cracen2& operator=(const Cracen2& other) = delete;

	Cracen2(Cracen2&& other) = default;
	Cracen2& operator=(Cracen2&& other) = default;


}; // End of class cracen2

} // End of namespace cracen2
