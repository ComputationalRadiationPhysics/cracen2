#include <map>
#include <string>

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/CoarseGrainedLocked.hpp"

using namespace cracen2::util;

int main(int , const char*[]) {

	using MapType = CoarseGrainedLocked<std::map<int, std::string>>;
	MapType map;

	JoiningThread writer([&map](){
		map.getView().get()[5] = "Hello World!";
	});
	{
		auto view = map.getReadOnlyViewOnChange([](const MapType::value_type& map){ return map.count(5) == 1; });
		std::cout << "map[5] = " << view.get().at(5) << std::endl;
	}

	return 0;
}
