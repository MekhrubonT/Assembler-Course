#include "single_thread_pool.h"

using namespace std;	

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

#include <iostream>

single_thread_pool::single_thread_pool() : is_shutdown(false),
 working_thread([this]() {
	while (!is_terminated()) {
		AUTOLOCK(lock);

		cond_var.wait(autolock, [this]() { 
									return !task_queue.empty() ||  
									is_terminated();
								});		
		if (is_terminated()) {
			break;
		}

		auto task = task_queue.front();
		task_queue.pop();
		autolock.unlock();
		
		// std::cout << "trying to resolve\n";
		task.first->resolve();
		task.second();
	}
}) {
 }

single_thread_pool::~single_thread_pool() {
}


void single_thread_pool::submit(http_request *task, function<void()> callback) {
	// std::cout << task->val << "\n";
	AUTOLOCK(lock);
	task_queue.emplace(task, callback);
	cond_var.notify_all();
	// std::cout << "submit finished\n";
}

bool single_thread_pool::is_terminated() {
	return is_shutdown;
}

void single_thread_pool::shutdown() {
	AUTOLOCK(lock);
	is_shutdown = true;
	cond_var.notify_all();
}
void single_thread_pool::await_termination() {
	shutdown();
	working_thread.join();	
}