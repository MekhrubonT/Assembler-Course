#ifndef FIXEDTHREADPOOL
#define FIXEDTHREADPOOL

#include "single_thread_pool.h"
#include <vector>
#include <unordered_map>

struct fixed_thread_pool
{
	fixed_thread_pool(std::size_t size);
	~fixed_thread_pool();

	fixed_thread_pool(const fixed_thread_pool& other) = delete;
	fixed_thread_pool& operator=(const fixed_thread_pool& other) = delete;

	void submit(std::shared_ptr<http_request> task, 
				std::function<void()> callback);

	void shutdown();
	void await_termination();

private:
	std::size_t size, cycle_go;
	std::vector<single_thread_pool> pool;
	std::unordered_map<http_request, std::size_t> storage;
};

#endif
