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

} // End of namespace util

} // End of namespace cracen2
