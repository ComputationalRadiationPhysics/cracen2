#pragma once

#include <string>
#include <iostream>
#include <chrono>

namespace cracen2 {

namespace util {

class TestSuite {
public:
	unsigned int result;
	std::string name;
	std::ostream& logger;
	TestSuite* parent;
	std::chrono::high_resolution_clock::time_point start;

	TestSuite(std::string name, std::ostream& logger = std::cerr, TestSuite* parent = nullptr);
	~TestSuite();

	unsigned int getResult();
	void test(bool assertion, std::string message);

	template <class T>
	void equal(const T& first, const T& second, std::string message);

	template <class T>
	void equalRange(const T& first, const T& second, std::string message);

}; // End of class TestSuite


template <class T>
void TestSuite::equal(const T& first, const T& second, std::string message) {
	if(first != second) {
		logger << "Comparision failed " << message << "("  << first << " != " << second << ")" << std::endl;
		result = 1;
	}
}

template <class T>
void TestSuite::equalRange(const T& first, const T& second, std::string message) {
	auto it1 = first.begin(), it2 = second.begin();
	while(it1 != first.end() && it2 != second.end()) {
		if(*it1 != *it2) {
			logger << "Range comparision failed " << message << "(Element"  << *it1 << " != " << *it2 << ")" << std::endl;
			return;
		}
		it1++;
		it2++;
	}
	if(it1 != first.end() && it2 != second.end()) {
		// Check if one is longer than the other
		logger << "Range comparision failed " << message << "(Ranges have different lengths.)" << std::endl;
	}
}
} // End of namespace util

} // End of namespace cracen2
