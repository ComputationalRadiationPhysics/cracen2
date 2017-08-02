#include "cracen2/util/Test.hpp"
#include "cracen2/network/Message.hpp"

#include <boost/optional/optional_io.hpp>

using namespace cracen2::util;
using namespace cracen2::network;

int main(int argc, char* argv[]) {

	TestSuite testSuite("Message");

	int i = 42;
	char c = 'x';
	float f = 3.1415;

	using TL = std::tuple<int, char, float>;
	using MyMessage = Message<TL>;
	using Visitor = typename MyMessage::Visitor;
	Visitor visitor(
		[&testSuite](int value){
			testSuite.equal(value, 42, "Visitor test for integer");
		},
		[&testSuite](char value){
			testSuite.equal(value, 'x', "Visitor test for char");
		},
		[&testSuite](float value){
			testSuite.equal(value, 3.1415f, "Visitor test for float");
		}
	);

	MyMessage(i).visit(visitor);
	MyMessage(c).visit(visitor);
	MyMessage(f).visit(visitor);

	testSuite.equal(MyMessage(i).cast<int>().get(), 42, "Cast test for integer");
	testSuite.equal(MyMessage(c).cast<char>().get(), 'x', "Cast test for char");
	testSuite.equal(MyMessage(f).cast<float>().get(), 3.1415f, "Cast test for float");

	testSuite.test(!MyMessage(i).cast<char>(), "Cast test for integer, with wrong type");
	testSuite.test(!MyMessage(i).cast<float>(), "Cast test for integer, with wrong type");
	testSuite.test(!MyMessage(c).cast<int>(), "Cast test for char, with wrong type");
	testSuite.test(!MyMessage(c).cast<float>(), "Cast test for char, with wrong type");
	testSuite.test(!MyMessage(f).cast<int>(), "Cast test for float, with wrong type");
	testSuite.test(!MyMessage(f).cast<char>(), "Cast test for float, with wrong type");

}
