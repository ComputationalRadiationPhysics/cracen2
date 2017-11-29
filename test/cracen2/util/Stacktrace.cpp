#include "cracen2/util/Stacktrace.hpp"
#include "cracen2/util/Test.hpp"
#include "cracen2/util/Signal.hpp"

using namespace cracen2::util;

void foo(int depth = 0) {
	if(depth > 3) {
		std::cout << cracen2::util::stacktrace << std::endl;
		return;
	}
	foo(depth+1);
}

int main(int, const char**) {


	TestSuite testSuite("Stacktrace TestSuite");

	foo();

	return 0;
}
