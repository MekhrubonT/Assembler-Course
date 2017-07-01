#include "fixed_thread_pool.h"
#include <iostream>
#include <functional>

using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)


int main() {

	fixed_thread_pool d(10);
	mutex m;
	for (int i = 1; i <= 100; ++i) {
		http_request* re = new http_request(i);
		d.submit(re, [&m](){
			// AUTOLOCK(m); 
			// std::cout << "ok\n";
		});
	}	
	for (int i = 1; i <= 10000000; ++i) {
	}
	std::cout << "Here\n";
		d.await_termination();
	std::cout << "Here\n";
	return 0;

}