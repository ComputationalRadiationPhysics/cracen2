#pragma once

#include <tuple>
#include <typeinfo>

namespace cracen2 {

namespace util {

template <class T, class... Types>
struct tuple_index;

template <class T, class... Types>
struct tuple_index<T, std::tuple<T, Types...>> {
	constexpr static size_t value = 0;
};

template <class T, class U, class... Types>
struct tuple_index<T, std::tuple<U, Types...>> {
	static_assert(sizeof...(Types) != 0, "Type T is not in Tuple");

	constexpr static size_t value = tuple_index<T, std::tuple<Types...>>::value + 1;
};

template <class T, class Tuple>
struct tuple_contains_type;

template <class T>
struct tuple_contains_type<T, std::tuple<>> {
	constexpr static bool value = false;
};

template <class T, class... TupleTypes>
struct tuple_contains_type<T, std::tuple<T, TupleTypes...>> {
	constexpr static bool value = true;
};

template <class T, class U, class... TupleTypes>
struct tuple_contains_type<T, std::tuple<U, TupleTypes...>> {
	constexpr static bool value = tuple_contains_type<T, std::tuple<TupleTypes...>>::value;
};

template <class Tuple>
struct tuple_duplicate_free;

template<>
struct tuple_duplicate_free<std::tuple<>> {
	constexpr static bool value = true;
};

template <class First, class... Rest>
struct tuple_duplicate_free<std::tuple<First, Rest...>> {
	constexpr static bool value =
		!tuple_contains_type<First, std::tuple<Rest...>>::value &&
		tuple_duplicate_free<std::tuple<Rest...>>::value;
};

template <class Tuple>
struct is_std_tuple {
	constexpr static bool value = false;

};

template <class... Types>
struct is_std_tuple<std::tuple<Types...>> {
	constexpr static bool value = true;

};

template <class Tuple>
struct tuple_get_type_names;

template <class... TupleArgs>
struct tuple_get_type_names<std::tuple<TupleArgs...>> {
	static const std::array<const char*, sizeof...(TupleArgs)> value() {
		return {{
			typeid(TupleArgs).name()...
		}};
	}
};

} // End of namespace util

} // End of namespace cracen2
