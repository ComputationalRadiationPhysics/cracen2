#pragma once

namespace cracen2 {

namespace util {

template <class... FunctorList>
struct Overload;

template <class Functor, class... FunctorList>
struct Overload<Functor, FunctorList...> :
	public Functor,
	public Overload<FunctorList...>
{
	Overload(Functor functor, FunctorList... functors) :
		Functor(functor),
		Overload<FunctorList...>(functors...)
	{};

	using Functor::operator();
	using Overload<FunctorList...>::operator();
};// End of struct Overload

template <>
struct Overload<> {
	void operator()() {};
}; // End of struct Overload

template <class... FunctorList>
Overload<FunctorList...> make_overload(FunctorList... functors) {
	return Overload<FunctorList...>(functors...);
} // End of function make_overload

} // End of namespace util

} // End of namespace cracen2
