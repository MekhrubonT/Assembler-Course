#include "setup.h"
#include "http_request.h"

#include <netdb.h>
#include <strings.h>
#include <memory.h>

#include <iostream>
#include <cassert>

using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

namespace {
	string parse_header(const string &request) {
		return request.substr(0, request.find(DCRLF) + DCRLF.size());
	}	
}

http_request::http_request(const string& request) : header(::parse_header(request)),
													body(request.substr(header.size(), request.size() - header.size())),

													 error(0) {

	parse_connection_type();
	parse_host();
	repair_header();
	remove_cache_and_set_encoding();
}

void http_request::repair_header() {
	if (get_error()) {
		return;
	}
	int pos = header.find(" ");
	int ddd = header.find(host) + host.size();
	header.erase(pos + 1, ddd - pos - 1);
}

void http_request::parse_connection_type() {
	log(header, "\n");
	if (header.size() >= 3 && header.substr(0, 3) == "GET") {
		return;
	}
	if (header.size() >= 4 && header.substr(0, 4) == "POST") {
		return;
	}

#ifdef DEBUG
	log("Only GET requests are supported\n");
#endif

	set_error(NOT_GET_REQUEST);
}

string remove(string d, string t) {
	auto st = d.find(t, 0);
	if (st != string::npos) {
	 	auto end = d.find(CRLF, st) + CRLF.size();
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
#ifdef DEFINE
		std::cout << "Host can't be found";
#endif
		set_error(NO_HOST_IN_REQUEST);
	} else {
		host_ind += 6;
		int endline = header.find(CRLF, host_ind);
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

void http_request::set_error(int flag) {
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
    	set_error(CANT_RESOLVE_HOST);
    } else {
    	set_server_addr(*res[0].ai_addr);
    	freeaddrinfo(res);
    }
}

bool http_request::is_request_ready(const std::string &example) {
	return example.find("\r\n\r\n") != std::string::npos;
}

bool http_request::is_response_finished(const std::string &example) {
	// std::cout << example << "\n";
	size_t pos;
	if (example.find("Transfer-Encoding: chunked") != string::npos) {				
		auto q = example.rfind("0" + DCRLF);
		if (q != std::string::npos && q + 5 == example.size()) {
			return true;
		}
	} else if ((pos = example.find("Content-Length: ")) != string::npos) {
		// log("example\n\n\n\n", example);
		pos += string{"Content-Length: "}.length();
		log(example.substr(pos, example.size() - pos), "\n");
		auto last = example.find(CRLF, pos);
		if (last != string::npos) {
			int leng = stoi(example.substr(pos, last - pos));
			auto body = example.find(DCRLF);
			if (body != string::npos && leng == (int)example.substr(body + DCRLF.size()).length()) {
				return true;
			}
		}
	}
	// std::cout << "response is not ready\n";
	return false;
}