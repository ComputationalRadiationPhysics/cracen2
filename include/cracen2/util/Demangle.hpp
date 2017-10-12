#pragma once

#include <typeinfo>
#include <string>

namespace cracen2 {

namespace util {

std::string demangle(const char* name);

template <class T>
std::string getTypeName() {
	return demangle(typeid(T).name());
}

} // End of namespace util

} // End of namespace cracen2
