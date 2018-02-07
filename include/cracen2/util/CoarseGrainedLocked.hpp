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
	std::atomic<unsigned int> writerWaiting;
	mutable std::mutex mutex;
	mutable std::condition_variable modified;

	mutable std::condition_variable readerExit;
	mutable std::atomic<unsigned int> reader;

public:

	class View{

		CoarseGrainedLocked& coarseGrainedLocked;
		std::unique_lock<std::mutex> lock;

	public:
		Type& view;
		View(CoarseGrainedLocked& coarseGrainedLocked, std::unique_lock<std::mutex> lock) :
			coarseGrainedLocked(coarseGrainedLocked),
			lock(std::move(lock)),
			view(coarseGrainedLocked.value)
		{}
		~View() {
			coarseGrainedLocked.modified.notify_all();
		}

		Type& get() {
			return coarseGrainedLocked.value;
		}

		Type* operator->() const {
			return &coarseGrainedLocked.value;
		}

		Type& operator*() const {
			return coarseGrainedLocked.value;
		}
	};

	class ReadOnlyView{
		const CoarseGrainedLocked& coarseGrainedLocked;
	public:

		ReadOnlyView(const CoarseGrainedLocked& coarseGrainedLocked) :
			coarseGrainedLocked(coarseGrainedLocked)
		{
			coarseGrainedLocked.reader++;
		}

		~ReadOnlyView() {
			coarseGrainedLocked.reader--;
			coarseGrainedLocked.readerExit.notify_all();
		}

		const Type& get() {
			return coarseGrainedLocked.value;
		}

		const Type* operator->() const {
			return &coarseGrainedLocked.value;
		}

		const Type& operator*() const {
			return coarseGrainedLocked.value;
		}

	};

	using value_type = Type;

	template <class... Args>
	CoarseGrainedLocked(Args... args) :
		value(args...),
		writerWaiting(false),
		reader(0)
	{}

	std::unique_ptr<View> getView() {
		writerWaiting++;
		std::unique_lock<std::mutex> lock(mutex);
		writerWaiting--;
		readerExit.wait(lock, [this](){ return reader == 0; });
		return std::unique_ptr<View>(new View(*this, std::move(lock)));
	}

	std::unique_ptr<ReadOnlyView> getReadOnlyView() const {
		std::unique_lock<std::mutex> lock(mutex); // Lock mutex for creation
		modified.wait(lock, [this](){ return writerWaiting == 0; });
		return std::unique_ptr<ReadOnlyView>(new ReadOnlyView(*this));
	}

	template <class Predicate>
	std::unique_ptr<ReadOnlyView> getReadOnlyView(Predicate predicate) const {
		std::unique_lock<std::mutex> lock(mutex); // Lock mutex for creation
		modified.wait(lock, [this, &predicate](){ return predicate(value) && (writerWaiting == 0); });
		return std::unique_ptr<ReadOnlyView>(new ReadOnlyView(*this));
	}

}; // End of class CoarseGrainedLocked

} // End of namespace util

} // End of namespace cracen2
