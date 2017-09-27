#include "cracen2/util/Test.hpp"

using namespace cracen2::util;

using hrc = std::chrono::high_resolution_clock;

TestSuite::TestSuite(std::string name, std::ostream& logger, TestSuite* parent) :
	result(0),
	name(name),
	logger(logger),
	parent(parent),
	start(hrc::now())
{
	logger << "Suite " << name << " started." << std::endl;
}

TestSuite::~TestSuite() {
	auto executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(hrc::now() - start);
	if(result == 0) logger << "Suite " << name << " passed. " << executionTime.count() << " (ms)" << std::endl;
	else logger << "Suite " << name << " failed. " << executionTime.count() << " (ms)" << std::endl;
	if(parent == nullptr) {
		if(result != 0) std::exit(result);
	} else {
		parent->result |= result;
	}
}

unsigned int TestSuite::getResult() {
	return result;
}

void TestSuite::test(bool assertion, std::string message) {
	if(!assertion) {
		logger << "Assertion failed. " << message << std::endl;
		result = 1;
	}
}

void TestSuite::fail(std::string message) {
	logger << "Assertion failed. " << message << std::endl;
	result = 1;
}
