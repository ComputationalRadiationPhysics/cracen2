#pragma once

#include <tuple>
#include <functional>

namespace cracen2 {

namespace util {

namespace detail {

template <class Funktor, class Result, class... Args>
std::tuple<Args...> paramList(Result(Funktor::*)(Args...) const) {
	return {};
}

template <class Funktor, class Result, class... Args>
std::tuple<Args...> paramList(Result(Funktor::*)(Args...)) {
	return {};
}

template <class Funktor, class Result, class... Args>
Result result(Result(Funktor::*)(Args...) const) {
	return {};
}

template <class Funktor, class Result, class... Args>
Result result(Result(Funktor::*)(Args...)) {
	return {};
}

} // End of namespace detail

template <class... Args>
struct FunctionInfo;

template <class R, class... Args>
struct FunctionInfo<R(Args...)> {
	using ParamList = std::tuple<Args...>;
	using Result = R;
};

template <class F>
struct FunctionInfo<F> {
	using ParamList = decltype(detail::paramList(&F::operator()));
	using Result = decltype(detail::result(&F::operator()));
};

} // End of namespace util

} // End of namespace cracen2
