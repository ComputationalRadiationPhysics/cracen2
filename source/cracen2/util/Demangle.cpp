#include "cracen2/util/Demangle.hpp"

#include <cxxabi.h>
#include <memory>

std::string cracen2::util::demangle(const char* name) {
	int status = -1;
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name;
}
