#include "setup.h"
#include "client_and_servers.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "http_request.h"

#include <iostream>
#include <errno.h>
#include <string.h>

#include <sys/timerfd.h>
#include <cassert>


file_desc::file_desc(int fd) : fd(fd) {}
file_desc::file_desc() : fd(-1) {}

file_desc::~file_desc() {
	if (fd != -1) {
		if (close(fd) == -1) {
			log("Error while closing ", fd, "\n");
		} else {
			log("Closed ", fd, "\n");
		}
	}
}

file_desc::file_desc(file_desc&& other) : 
	fd(other.fd) {
	other.fd = -1;
}

file_desc& file_desc::operator=(file_desc&& other) { 
	fd = other.fd; 
	other.fd = -1;
	return *this;
}

int file_desc::get_fd() {
	return fd;
}

int file_desc::release() {
	int temp = fd;
	fd = -1;
	return temp;
}

// SOCKET_WRAPPER
socket_wrapper::socket_wrapper() : socket_wrapper(-1) {}
socket_wrapper::socket_wrapper(int fd) : file_desc(fd) {}

socket_wrapper::socket_wrapper(int domain, int type, int protocol) 
				: socket_wrapper(socket(domain, type, protocol)) {}

socket_wrapper socket_wrapper::accept() {
	sockaddr_in cli_addr;
    socklen_t socklen = sizeof(cli_addr);
	socket_wrapper accepted(accept4(fd, 
		reinterpret_cast<sockaddr*>(&cli_addr), &socklen, SOCK_NONBLOCK | SOCK_CLOEXEC));
	return accepted;
}


std::string socket_wrapper::read(size_t buffer_size) {
	char buf[buffer_size];
	int kol = ::read(fd, buf, buffer_size);
	// assert(kol != -1);
	return std::string(buf, buf + kol);
}

#include <strings.h>
#include <errno.h>

int socket_wrapper::write(const std::string& data) {
	int res = ::write(fd, data.c_str(), data.size());
	// std::cout << "res=" << res << "\n";
	return res;
}


// TIMER_FD 
timer_fd::timer_fd() : file_desc(timerfd_create(CLOCK_REALTIME, 0)) {}

// DEFAULT 2 seconds
void timer_fd::reset(long ns) {
	struct itimerspec new_value;
	new_value.it_value.tv_sec = ns / 1000000000;
	new_value.it_value.tv_nsec = ns % 1000000000;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_nsec = 0;

	timerfd_settime(fd, 0, &new_value, NULL);
}


client::client(int fd) : socket(fd), ser(nullptr) {}


int client::get_fd() {
	return socket.get_fd();
}
int client::get_ser_fd() {
	return ser->get_fd();
}

std::string client::get_data() {
	return data;
}
void client::set_data(const std::string& data) {
	this->data = data;
}

bool client::has_server() {
	return ser != nullptr;
}

void client::bind(client* other) {
	assert(ser == nullptr);
	assert(other->ser == nullptr);
	ser = other;
	other->ser = this;
}
void client::unbind() {
	ser = nullptr;
}

void client::set_message(const std::string& msg) {
	data = msg;
}

int client::read(size_t sz) {
	try {
		std::string d(socket.read(sz));
		data += d;
		return d.size();
	} catch (...) {
		return -1;
	}
}

int client::write() {
	if (data.empty() && ser) {
		data = ser->data;
		ser->data.clear();
	}
	if (data.empty()) {
		return 0;
	}
	auto d = socket.write(data);
	if (d == -1) {
		log(strerror(errno), "\n");
		return errno == EPIPE ? -1 : 0;
	}
	data = data.substr(d, data.size() - d);
	return d;
}