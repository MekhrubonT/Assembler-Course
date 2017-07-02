#ifndef HTTP_WRAPPER_H
#define HTTP_WRAPPER_H

#include <sys/socket.h>
#include <functional>

struct http_request
{
	http_request(const std::string& request);
	http_request& operator=(const http_request& other) = delete;

	bool operator==(const http_request& other) const;


	std::string get_request() const;

	static bool is_request_ready(const std::string &request);
	static bool is_response_finished(const std::string &request);

	bool get_error() const;
	sockaddr get_server() const;


	// Needed for hashing requests 
	void set_client(int client);
	int get_client() const;

	// resolve
	void resolve();


private: 
	void parse_connection_type();
	void parse_host();
	void parse_header();
	void remove_cache_and_set_encoding();
	void repair_header();


	void set_error(int flag);
	void set_server_addr(const sockaddr& addr);

	int client_id;

	std::string header;
	std::string body;
	std::string host;


	sockaddr server_addr;
	
	int error;
	size_t port_delim;
};

namespace std {
	template<> 
	struct hash<http_request> 
	{
		std::size_t operator()(const http_request& http_request) const {
			return http_request.get_client();
		}
	};
}

#endif