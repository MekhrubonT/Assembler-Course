#include "fixed_thread_pool.h"

using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

fixed_thread_pool::fixed_thread_pool(size_t size) : size(size), cycle_go(0), pool(size) {
}

fixed_thread_pool::~fixed_thread_pool() {}

void fixed_thread_pool::submit(shared_ptr<http_request> task, function<void()> callback) {
	auto iterator = storage.find(*task);
	size_t index;

	if (iterator == storage.end()) {
		storage[*task] = index = cycle_go++ % size;
	} else {
		index = iterator->second;
	}

	pool[index].submit(task, callback);
}

void fixed_thread_pool::shutdown() {
	for (auto &x : pool) {
		x.shutdown();
	}
}
void fixed_thread_pool::await_termination() {
	shutdown();
	for (auto &x : pool) {
		x.await_termination();
	}
}
