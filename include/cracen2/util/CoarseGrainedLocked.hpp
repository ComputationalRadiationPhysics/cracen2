#pragma once

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

		CoarseGrainedLocked* coarseGrainedLocked;
		std::lock_guard<std::mutex> lock;

	public:
		Type& view;
		View(CoarseGrainedLocked* coarseGrainedLocked) :
			coarseGrainedLocked(coarseGrainedLocked),
			view(coarseGrainedLocked->value),
			lock(coarseGrainedLocked->mutex)
		{}
		~View() {
			coarseGrainedLocked->modified.notify_all();
		}

		Type& get() {
			return coarseGrainedLocked->value;
		}
	};

	class ReadOnlyView{
		CoarseGrainedLocked* coarseGrainedLocked;
	public:

		ReadOnlyView(CoarseGrainedLocked* coarseGrainedLocked) :
			coarseGrainedLocked(coarseGrainedLocked)
		{
			std::lock_guard<std::mutex> lock(coarseGrainedLocked->readerLock);
			coarseGrainedLocked->reader++;
		}

		~ReadOnlyView() {
			std::lock_guard<std::mutex> lock(coarseGrainedLocked->readerLock);
			coarseGrainedLocked->reader--;
			coarseGrainedLocked->readerExit.notify_all();
		}

		const Type& get() {
			return coarseGrainedLocked->value;
		}
	};

public:
	using value_type = Type;

	template <class... Args>
	CoarseGrainedLocked(Args... args) :
		value(args...),
		reader(0)
	{}

	View getView() {
		std::lock_guard<std::mutex> lock(readerMutex);
		readerExit.wait(lock, [this](){ return reader == 0; });
		return View(this);
	}

	ReadOnlyView getReadOnlyView() {
		std::lock_guard<std::mutex> lock(mutex); // Lock mutex for creation
		return ReadOnlyView(this);
	}

	template <class Predicate>
	ReadOnlyView getReadOnlyViewOnChange(Predicate predicate = [](const value_type&){ return true; }) {
		std::lock_guard<std::mutex> lock(mutex); // Lock mutex for creation
		modified.wait(lock, [this, &predicate](){ return predicate(value); });
		return getReadOnlyView();
	}

}; // End of class CoarseGrainedLocked

} // End of namespace util

} // End of namespace cracen2
