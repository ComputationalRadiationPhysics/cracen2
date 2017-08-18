#include "cracen2/util/Tuple.hpp"
#include "cracen2/util/Test.hpp"

#include <vector>
#include <string>

struct Foo {};

using namespace cracen2::util;

int main() {

	TestSuite testSuite("Tuple");

	using Tuple = std::tuple<int, char, float, std::vector<int>, Foo, std::string>;
	auto mangledNames = tuple_get_type_names<Tuple>::value();

	testSuite.test(tuple_contains_type<int, Tuple>::value, "tuple contains int");
	testSuite.test(tuple_contains_type<char, Tuple>::value, "tuple contains char");
	testSuite.test(tuple_contains_type<float, Tuple>::value, "tuple contains float");
	testSuite.test(tuple_contains_type<std::vector<int>, Tuple>::value, "tuple contains std::vector<int>");
	testSuite.test(tuple_contains_type<Foo, Tuple>::value, "tuple contains Foo");
	testSuite.test(tuple_contains_type<std::string, Tuple>::value, "tuple contains std::string");
	testSuite.test(!tuple_contains_type<double, Tuple>::value, "tuple does not contains double");

	testSuite.test(tuple_duplicate_free<std::tuple<>>::value, "std::tuple<> is free of duplicate types");
	testSuite.test(tuple_duplicate_free<std::tuple<int>>::value, "std::tuple<int> is free of duplicate types");
	testSuite.test(tuple_duplicate_free<std::tuple<int, float>>::value, "std::tuple<int, float> is free of duplicate types");

	testSuite.test(tuple_duplicate_free<Tuple>::value, "Tuple is free of duplicate types");
	testSuite.test(!tuple_duplicate_free<std::tuple<int, float, char, double, std::string, short, double>>::value, "Tuple is not free of duplicate types");
	testSuite.test(!tuple_duplicate_free<std::tuple<int, float, char, double, int, std::string, short>>::value, "Tuple is not free of duplicate types");
	testSuite.test(!tuple_duplicate_free<std::tuple<int, float, char, double, std::string, short, short>>::value, "Tuple is not free of duplicate types");

	testSuite.equal(static_cast<int>(tuple_index<int, Tuple>::value), 				0, "index of int");
	testSuite.equal(static_cast<int>(tuple_index<char, Tuple>::value), 				1, "index of char");
	testSuite.equal(static_cast<int>(tuple_index<float, Tuple>::value), 			2, "index of float");
	testSuite.equal(static_cast<int>(tuple_index<std::vector<int>, Tuple>::value),	3, "index of std::vector<int>");
	testSuite.equal(static_cast<int>(tuple_index<Foo, Tuple>::value), 				4, "index of Foo");
	testSuite.equal(static_cast<int>(tuple_index<std::string, Tuple>::value),		5, "index of std::string");

	testSuite.test(is_std_tuple<Tuple>::value, "Tuple is a std::tuple<...>");
	testSuite.test(is_std_tuple<std::tuple<>>::value, "std::tuple<> is a std::tuple<...>");
	testSuite.test(!is_std_tuple<double>::value, "double is not a std::tuple<...>");

	testSuite.equal(mangledNames[0], typeid(int).name(), "tuple_get_type_names test");
	testSuite.equal(mangledNames[1], typeid(char).name(), "tuple_get_type_names test");
	testSuite.equal(mangledNames[2], typeid(float).name(), "tuple_get_type_names test");
	testSuite.equal(mangledNames[3], typeid(std::vector<int, std::allocator<int> >).name(), "tuple_get_type_names test");
	testSuite.equal(mangledNames[4], typeid(Foo).name(), "tuple_get_type_names test");
	testSuite.equal(mangledNames[5], typeid(std::string).name(), "tuple_get_type_names test");

}
