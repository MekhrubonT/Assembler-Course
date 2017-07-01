#ifndef SINGLETHREADPOOL
#define SINGLETHREADPOOL

#include "../http_request.h"
#include <condition_variable>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>



// Thread-safe
struct single_thread_pool
{
	single_thread_pool();
	~single_thread_pool();

	single_thread_pool(const single_thread_pool& other) = delete;
	single_thread_pool& operator=(const single_thread_pool& other) = delete;


	void submit(std::shared_ptr<http_request> task, std::function<void()> callback);
	void shutdown();
	void await_termination();

	bool is_terminated();

private:
	std::mutex lock;
    std::condition_variable cond_var;
	
	bool is_shutdown;
	std::queue<std::pair<std::shared_ptr<http_request>, std::function<void()>>> task_queue;
	std::thread working_thread;
};

#endif