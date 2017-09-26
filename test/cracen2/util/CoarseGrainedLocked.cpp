#include <map>
#include <string>

#include "cracen2/util/Test.hpp"
#include "cracen2/util/Thread.hpp"
#include "cracen2/util/CoarseGrainedLocked.hpp"

using namespace cracen2::util;

int main(int , const char*[]) {

	TestSuite testSuite("CoarseGrainedLocked");

	using MapType = CoarseGrainedLocked<std::map<int, std::string>>;
	MapType map;

	/*
	 * Test concurrent readonly access
	 */

	{
		auto view1 = map.getReadOnlyView();
		auto view2 = map.getReadOnlyView();
	}

	/*
	 * Test conditional readonly view
	 */

	JoiningThread writer([&map](){
		map.getView()->get()[5] = "Hello World!";
	});
	{
		auto view = map.getReadOnlyView([](const MapType::value_type& map){
			return map.count(5) == 1;
		});
		testSuite.equal(view->get().at(5), std::string("Hello World!"), "Access to map failed.");
	}


	/*
	 * Test concurrent read/write access (should fail)
	 */

// 	DetatchingThread ([&](){
// 		auto view1 = map.getView();
// 		auto view2 = map.getView();
// 		testSuite.fail("It should not be possible to get two views at the same time");
// 	});


	/*
	 * Test concurrent readonly and read/write access (should fail)
	 */

// 	DetatchingThread ([&](){
// 		auto view1 = map.getReadOnlyView();
// 		auto view2 = map.getView();
// 		testSuite.fail("It should not be possible to get a view and a readonly view at the same time");
// 	});

	//std::this_thread::sleep_for(std::chrono::seconds(1));

}
