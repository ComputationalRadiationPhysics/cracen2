#pragma once

#include "cracen2/util/Unused.hpp"
#include <initializer_list>

namespace cracen2 {

namespace util {

namespace detail {

template <class Function, class Arg>
int run(Function&& f, Arg&& arg) {
	f(std::forward<Arg>(arg));
	return 0;
}

} // End of namespace detail

template <class Function, class... Args>
void foreach(Function&& function, Args&&... args) {
	UNUSED(
		std::initializer_list<int>{
			detail::run(std::forward<Function>(function), std::forward<Args>(args))...
		}
	);
}

template <class Functor, size_t I = 0, class... Types>
void tuple_foreach(std::tuple<Types...>&, Functor&&);

template <class Functor, size_t I>
void tuple_foreach(std::tuple<>&, Functor&&) {};

template <class Functor, size_t I = 0, class T1, class... Types>
void tuple_foreach(std::tuple<T1, Types...>& t, Functor&& f) {
	f(std::get<I>(t));
	tuple_foreach<Functor, I+1, Types...>(t, std::forward<Functor>(f));
}



} // End of namespace util

} // End of namespace cracen2
