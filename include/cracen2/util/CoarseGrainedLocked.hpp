#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace cracen2 {

namespace util {

template <class Type>
class CoarseGrainedLocked {

	Type value;
	std::mutex mutex;
	std::condition_variable modified;

	std::mutex readerMutex;
	std::condition_variable readerExit;
	unsigned int reader;

	class View{

		CoarseGrainedLocked& coarseGrainedLocked;
		std::unique_lock<std::mutex> lock;

	public:
		Type& view;
		View(CoarseGrainedLocked& coarseGrainedLocked) :
			coarseGrainedLocked(coarseGrainedLocked),
			lock(coarseGrainedLocked.mutex),
			view(coarseGrainedLocked.value)
		{}
		~View() {
			coarseGrainedLocked.modified.notify_all();
		}

		Type& get() {
			return coarseGrainedLocked.value;
		}
	};

	class ReadOnlyView{
		CoarseGrainedLocked& coarseGrainedLocked;
	public:

		ReadOnlyView(CoarseGrainedLocked& coarseGrainedLocked) :
			coarseGrainedLocked(coarseGrainedLocked)
		{
			std::unique_lock<std::mutex> lock(coarseGrainedLocked.readerMutex);
			coarseGrainedLocked.reader++;
		}

		~ReadOnlyView() {
			std::unique_lock<std::mutex> lock(coarseGrainedLocked.readerMutex);
			coarseGrainedLocked.reader--;
			coarseGrainedLocked.readerExit.notify_all();
		}

		const Type& get() {
			return coarseGrainedLocked.value;
		}
	};

public:
	using value_type = Type;

	template <class... Args>
	CoarseGrainedLocked(Args... args) :
		value(args...),
		reader(0)
	{}

	std::unique_ptr<View> getView() {
		std::unique_lock<std::mutex> lock(readerMutex);
		readerExit.wait(lock, [this](){ return reader == 0; });
		return std::unique_ptr<View>(new View(*this));
	}

	std::unique_ptr<ReadOnlyView> getReadOnlyView() {
		std::unique_lock<std::mutex> lock(mutex); // Lock mutex for creation
		return std::unique_ptr<ReadOnlyView>(new ReadOnlyView(*this));
	}

	template <class Predicate>
	std::unique_ptr<ReadOnlyView> getReadOnlyViewOnChange(Predicate predicate = [](const value_type&){ return true; }) {
		std::unique_lock<std::mutex> lock(mutex); // Lock mutex for creation
		modified.wait(lock, [this, &predicate](){ return predicate(value); });
		return std::unique_ptr<ReadOnlyView>(new ReadOnlyView(*this));
	}

}; // End of class CoarseGrainedLocked

} // End of namespace util

} // End of namespace cracen2
