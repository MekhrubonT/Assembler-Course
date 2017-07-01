#include "http_request.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>

#include <iostream>
#include <memory.h>
#include <errno.h>


using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

http_request::http_request(int val) : val(val) {
}

bool http_request::operator==(const http_request& other) const {
	return this == &other;
}

void http_request::resolve() {
	static mutex m;
	AUTOLOCK(m);
	std::cout << val * val << "\n";
}

