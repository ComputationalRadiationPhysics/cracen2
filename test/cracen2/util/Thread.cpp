#include <iostream>
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/Test.hpp"

using namespace cracen2::util;

int main() {
	TestSuite testSuite("ThreadTest");

	try {

		JoiningThread jt([](){
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		});
		DetatchingThread dt([](){
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		});
	} catch (const std::exception& e) {
		testSuite.test(false, "Thread execution and destruction");
		return 1;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	return 0;
}
