#include "cracen2/util/Demangle.hpp"
#include "cracen2/util/Test.hpp"

#include <vector>


struct Foo {

};

struct Bar : public Foo {

};
using namespace cracen2::util;

int main() {
	TestSuite testSuite("Demangle");

	testSuite.equal(demangle(typeid(int).name()), std::string("int"), "");
	testSuite.equal(demangle(typeid(Foo).name()), std::string("Foo"), "");
	testSuite.equal(demangle(typeid(Bar).name()), std::string("Bar"), "");
	testSuite.equal(demangle(typeid(std::vector<int>).name()), std::string("std::vector<int, std::allocator<int> >"), "");

}
