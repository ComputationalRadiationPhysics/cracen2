#include "cracen2/util/Test.hpp"
#include "cracen2/network/Message.hpp"

#include <boost/optional/optional_io.hpp>

using namespace cracen2::util;
using namespace cracen2::network;

int main() {

	TestSuite testSuite("Message");

	int i = 42;
	char c = 'x';
	float f = 3.1415;

	using TL = std::tuple<int, char, float>;
	using MyMessage = Message<TL>;

	auto visitor = MyMessage::make_visitor(
		[&testSuite](int value) -> int{
			testSuite.equal(value, 42, "Visitor test for integer");
			return 0;
		},
		[&testSuite](char value) -> int{
			testSuite.equal(value, 'x', "Visitor test for char");
			return 1;
		},
		[&testSuite](float value) -> int {
			testSuite.equal(value, 3.1415f, "Visitor test for float");
			return 2;
		}
	);

	testSuite.equal(MyMessage(i).visit(visitor), 0, "Visitor return test.");
	testSuite.equal(MyMessage(c).visit(visitor), 1, "Visitor return test.");
	testSuite.equal(MyMessage(f).visit(visitor), 2, "Visitor return test.");

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
