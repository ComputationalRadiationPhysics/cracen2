#include "cracen2/util/Function.hpp"
#include "cracen2/util/Test.hpp"

using namespace cracen2::util;

struct Functor {
	char operator()(int, char, float) {
		return 'f';
	}
};

auto lambda = [](int, char, float) -> char { return 'l'; };

char freeFunction(int, char, float) {
	return 'p';
}

int main() {
	TestSuite testSuite("Function");
	lambda(0,0,0);

	using arity = std::tuple<int, char, float>;
	using result = char;

	testSuite.test(std::is_same<FunctionInfo<Functor>::ParamList, arity>::value, "Functor params");
	testSuite.test(std::is_same<FunctionInfo<Functor>::Result, result>::value, "Functor result");

	testSuite.test(std::is_same<FunctionInfo<decltype(freeFunction)>::ParamList, arity>::value, "function pointer params");
	testSuite.test(std::is_same<FunctionInfo<decltype(freeFunction)>::Result, result>::value, "function pointer result");

	testSuite.test(std::is_same<FunctionInfo<decltype(lambda)>::ParamList, arity>::value, "lambda params");
	testSuite.test(std::is_same<FunctionInfo<decltype(lambda)>::Result, result>::value, "lambda result");

}
