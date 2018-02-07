#include "cracen2/util/Stacktrace.hpp"
#include "cracen2/util/Demangle.hpp"

#include <execinfo.h>
#include <dlfcn.h>

#include <cstdlib>
#include <cstdio>

#include <functional>
#include <vector>
#include <sstream>
#include <memory>

std::ostream& cracen2::util::stacktrace(std::ostream& os) {
	std::vector<void*> buffer(1);
	std::size_t size;
	do {
		buffer.resize(buffer.size()*2);
		size = backtrace (buffer.data(), buffer.size());
	} while(size == buffer.size());
	buffer.resize(size);

	os <<"Obtained " << buffer.size() << " stack frames.\n";

	for (std::size_t i = 0; i < size; i++) {
		Dl_info info;
		dladdr(buffer[i], &info);
		os << "["<< i << "]:" << cracen2::util::demangle(info.dli_sname) << " in ";
		std::stringstream command;
		std::stringstream offset;
		offset << std::hex << "0x" << (static_cast<char*>(info.dli_saddr) - static_cast<char*>(info.dli_fbase));
		command << "addr2line "
// 			<< "-j .text "
			<< " -e " << info.dli_fname << " "
			<<  offset.str();
// 		os << command.str() << "\n";

		std::unique_ptr<FILE, std::function<void(FILE*)>> stream(
			popen(command.str().c_str(), "r"),
			[](FILE* f) -> void {
				pclose(f);
			}
		);
		std::array<char, 128> buffer;
		while(!feof(stream.get())) {
			if (fgets(buffer.data(), buffer.size(), stream.get()) != nullptr)
            os << buffer.data();
		}
	}

	return os;
}
