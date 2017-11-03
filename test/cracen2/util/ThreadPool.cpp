#include <atomic>

#include "cracen2/util/ThreadPool.hpp"
#include "cracen2/util/Test.hpp"

using namespace cracen2::util;

constexpr unsigned int runs = 1000;
int main(int, char**) {
	TestSuite test("ThreadPool Suite");

	std::atomic<unsigned int> counter { 0 };

	{
		ThreadPool pool(8);

		for(unsigned int i = 0; i < runs; i++) {
			pool.async([&counter]() {
				counter++;
				return 0;
			});
		}
	}

	test.equal(static_cast<unsigned int>(counter), runs, "All tasks executed");

	return 0;
};
