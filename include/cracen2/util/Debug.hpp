#pragma once

#include <iostream>

namespace cracen2 {

namespace util {

constexpr bool debug = true;
extern std::ostream& logStream;

} // End of namespace util

} // End of namespace cracen2

#define Log if(cracen2::util::debug) std::cout << "debug test"; std::cout << __FILE__ << ":" << __LINE__ << " "
