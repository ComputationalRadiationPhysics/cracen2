#pragma once

#include <boost/predef.h>

#if BOOST_COMP_GNUC
#define GCC_WORKAROUND(x)\
template <class T>\
std::function<void(T)> createVisitorLambda() {\
	return [this](T element){\
		constexpr size_t id = util::tuple_index<util::AtomicQueue<T>, QueueType>::value;\
		auto& queue = std::get<id>(outputQueues);\
		queue.push(element);\
	};\
}\
\
void receiver() {\
	bool running = true;\
	auto visitor = typename ClientType::DataVisitor(\
		[&running](backend::CracenClose){\
			running = false;\
		},\
		createVisitorLambda<MessageTypeList>()...\
	);\
	while(client.isRunning() && running) {\
		try {\
			client.receive(visitor);\
		} catch(const std::exception&) {\
			return;\
		}\
	}\
}\

#else

#define GCC_WORKAROUND(x) x

#endif
