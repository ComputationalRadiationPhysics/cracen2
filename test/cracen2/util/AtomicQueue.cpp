#include "cracen2/util/AtomicQueue.hpp"
#include "cracen2/util/Thread.hpp"

#include <set>
#include <vector>
#include <atomic>
#include <future>
#include <iostream>

std::ostream& operator<<(std::ostream& lhs, const std::set<int>& rhs) {
	lhs << "[";
	for(int i : rhs) {
		lhs << i << ", ";
	}
	lhs << "]";
	return lhs;
}

#include "cracen2/util/Test.hpp"


using namespace cracen2::util;
constexpr int runs = 100000;


void sequenceTest(TestSuite& testSuite) {
	AtomicQueue<int> queue(100);

	JoiningThread producer([&](){
		for(int i = 0; i < runs; i++) {
			queue.push(i);
		}
	});


	JoiningThread consumer([&](){
		for(int i = 0; i < runs; i++) {
			int j = queue.pop();
			testSuite.equal(i, j, "sequence test");
		}
	});
}

void multiConsumer(TestSuite& testSuite) {
	AtomicQueue<int> queue(100);

	JoiningThread producer([&](){
		for(int i = 0; i < runs; i++) {
			queue.push(i);
		}
	});

	std::atomic<int> count(0);

	std::vector<JoiningThread> consumer;
	for(int i = 0; i < 10; i++) {
		consumer.push_back(
			JoiningThread([&](){
				while(queue.tryPop(std::chrono::milliseconds(200))) {
					count++;
				}
			})
		);
	}
	consumer.clear();

	testSuite.equal(runs, static_cast<int>(count), "multiple consumer test");
}

void multiProducer(TestSuite& testSuite) {

	AtomicQueue<int> queue(100);
	std::atomic<int> in(0), out(0);

	std::vector<JoiningThread> producer;
	for(int i = 0; i < 10; i++) {
		producer.push_back(
			JoiningThread([&](){
				while(int c = in.fetch_add(1) < runs) {
					queue.push(c);
				}
			})
		);
	}

	std::vector<JoiningThread> consumer;
	consumer.push_back(
		JoiningThread([&](){
			while(queue.tryPop(std::chrono::milliseconds(200))) {
				out++;
			}
		})
	);
	consumer.clear();
	producer.clear();

	testSuite.equal(static_cast<int>(out), static_cast<int>(runs), "multiple producer test");

}

void multiProucerConsumer(TestSuite& testSuite) {
	AtomicQueue<int> queue(100);
	const int runs = 100;
	std::atomic<int> in(0), out(0);

	std::vector<JoiningThread> producer;
	for(int i = 0; i < 10; i++) {
		producer.push_back(
			JoiningThread([&](){
				int c = in++;
				while(c < runs) {
					queue.push(c);
					c = in++;
				}
			})
		);
	}


	std::vector<std::future<std::set<int>>> outputs;
	for(unsigned int i = 0; i < 10; i++) {
		outputs.push_back(
			std::async(std::launch::async, [&]() -> std::set<int> {
				std::set<int> result;
				while(boost::optional<int> optional = queue.tryPop(std::chrono::milliseconds(200))) {
					result.insert(optional.get());
				}
				return result;
			})
		);
	}

	producer.clear();
	std::set<int> result1, result2;
	for(auto& future : outputs) {
		auto part = future.get();
		for(int i : part) {
			result1.insert(i);
		}
	}
	for(int i = 0; i < runs; i++) {
		result2.insert(i);
	}
	testSuite.equal(result1, result2, "input/output comparation");
}


int main(int argc, char* argv[]) {
	TestSuite testSuite("AtomicQueueTest");

	sequenceTest(testSuite);
	multiConsumer(testSuite);
	multiProducer(testSuite);
	multiProucerConsumer(testSuite);

}
