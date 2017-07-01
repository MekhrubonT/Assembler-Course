#ifndef HTTP_WRAPPER_H
#define HTTP_WRAPPER_H

#include <string>
#include <sys/socket.h>
#include <mutex>

struct http_request
{
	http_request(int val);

	http_request& operator=(const http_request& other) = delete;

	bool operator==(const http_request& other) const;
	// resolve
	void resolve();


	int val;
private: 
};

namespace std {
	template<> 
	struct hash<http_request> 
	{
		std::size_t operator()(const http_request& http_request) const {
			return http_request.val;
		}
	};
}


struct http_response
{
	
};

#endif