#include "cracen2/network/BufferAdapter.hpp"
#include "cracen2/network/adapter/All.hpp"
#include "cracen2/util/Test.hpp"

using namespace cracen2::network;
using namespace cracen2::util;

struct Foo {
	int bar;
};

int main() {
	TestSuite testSuite("BufferAdapter");

	{
		constexpr int goal = 42;
		int value = goal;
		ImmutableBuffer buffer = make_buffer_adaptor(value);

		int result = 0;
		BufferAdapter<int>(buffer).copyTo(result);
		testSuite.equal(goal, result, "Buffer Adaptor for trivial type(int)");
	}

	{
		Foo foo{42};
		ImmutableBuffer buffer = make_buffer_adaptor(foo);

		Foo result{0};
		BufferAdapter<Foo>(buffer).copyTo(result);
		testSuite.equal(result.bar, Foo{42}.bar, "Buffer Adaptor for struct");
	}

	{
		const std::string value = "Hello World!";
		ImmutableBuffer buffer = make_buffer_adaptor(value);

		std::string result = "";
		BufferAdapter<std::string>(buffer).copyTo(result);
		testSuite.equal(result, std::string("Hello World!"), "Buffer Adaptor for std::string");
	}

	{
		std::string value = "Hello World!";
		ImmutableBuffer buffer = make_buffer_adaptor(value);

		std::string result = "";
		BufferAdapter<std::string>(buffer).copyTo(result);
		testSuite.equal(result, std::string("Hello World!"), "Buffer Adaptor for std::string");
	}

	{
		std::vector<int> value = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
		ImmutableBuffer buffer = make_buffer_adaptor(value);

		std::vector<int> result = {};
		BufferAdapter<std::vector<int>>(buffer).copyTo(result);
		testSuite.equalRange(result, std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "Buffer Adaptor for std::vector");
	}

	{
		using TestType = std::array<int, 10000>;
		TestType value = { {1, 2, 3, 4, 5, 6, 7, 8, 9, 10} };
		ImmutableBuffer buffer = make_buffer_adaptor(value);

		TestType result = {};
		BufferAdapter<TestType>(buffer).copyTo(result);
		testSuite.equalRange(result, { {1, 2, 3, 4, 5, 6, 7, 8, 9, 10} }, "Buffer Adaptor for std::array");
	}



}
