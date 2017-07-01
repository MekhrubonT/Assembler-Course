#include "http_request.h"

#include <netdb.h>
#include <strings.h>
#include <memory.h>

#include <iostream>

using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

namespace {
	string parse_header(const string &request) {
		return request.substr(0, request.find("\r\n\r\n", 0) + 2);
	}	
}

http_request::http_request(const string& request) : header(::parse_header(request)),
													body(request.substr(header.size(), request.size() - header.size())),

													 error(false) {
	parse_connection_type();
	parse_host();
	remove_cache_and_set_encoding();
}

void http_request::parse_connection_type() {
	std::cout << header << "\n";
	if (header.size() >= 3 && header.substr(0, 3) == "GET") {
		return;
	}
	if (header.size() >= 4 && header.substr(0, 4) == "POST") {
		return;
	}

	std::cout << "Only GET requests are supported\n";
	set_error(true);

}

string remove(string d, string t) {
	auto st = d.find(t, 0);
	if (st != string::npos) {
	 	auto end = d.find("\r\n", st) + 2;
		d.erase(st, end - st);
	}
	return d;
}

void http_request::remove_cache_and_set_encoding() {
	header = remove(header, "If-Modified-Since:");
	header = remove(header, "If-None-Match:");
}

bool http_request::operator==(const http_request& other) const {
	return this == &other;
}

void http_request::parse_host() {
	size_t host_ind = header.find("Host:");
	if (host_ind == string::npos) {
		std::cout << "Host can't be found";
		set_error(true);
	} else {
		host_ind += 6;
		int endline = header.find("\r\n", host_ind);
		host = header.substr(host_ind, endline - host_ind);
		port_delim = host.find(":");
	}
}

string http_request::get_request() const {
	return header + body;
}

void http_request::set_server_addr(const sockaddr& addr) {
	server_addr = addr;
}
sockaddr http_request::get_server() const {
	return server_addr;
}

void http_request::set_error(bool flag) {
	error = flag;
}
bool http_request::get_error() const {
	return error;
}

void http_request::set_client(int client) {
	client_id = client;
}

int http_request::get_client() const {
	return client_id;
}


void http_request::resolve() {
	if (get_error()) {
		return;
	}
	addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    
	string name = host.substr(0, port_delim);
	string port = port_delim == string::npos ? "80" : host.substr(port_delim + 1);

    int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &res);

    if (error) {
    	set_error(true);
    } else {
    	set_server_addr(*res[0].ai_addr);
    	freeaddrinfo(res);
    }
}

bool http_request::is_request_ready(const std::string &example) {
	return example.find("\r\n\r\n") != std::string::npos;
}