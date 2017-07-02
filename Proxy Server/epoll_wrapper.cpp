#include "epoll_wrapper.h"


#include "http_request.h"
#include <netinet/in.h>

#include <iostream>
#include <errno.h>
#include <cassert>
#include <string.h>


epoll_wrapper::epoll_wrapper() {}

epoll_wrapper::epoll_wrapper(int flags) : file_desc(epoll_create1(flags)) {}

epoll_wrapper::~epoll_wrapper() {}


void epoll_wrapper::add_event(int fd, int flags, std::function<void(epoll_event&)> handle) {
	while (change_client(fd, EPOLL_CTL_ADD, flags) == -1);
	handlers.emplace(id(fd, flags), handle);
}

void epoll_wrapper::del_event(int fd, int flags) {
	while (change_client(fd, EPOLL_CTL_DEL, flags) != -1);
	handlers.erase(id(fd, flags));
}

void epoll_wrapper::del_event(const epoll_event& event) {
	del_event(event.data.fd, event.events);
}

int epoll_wrapper::change_client(int clfd, int change_op, int flags) {
	epoll_event client_event;
	client_event.data.fd = clfd;
	client_event.events = flags;

    return epoll_ctl(fd, change_op, clfd, &client_event);
}

void epoll_wrapper::execute() {
	// std::cout << "Waiting\n";
	epoll_event events[10];
	int amount = epoll_wait(fd, events, 10, -1);
	invalid.clear();

	static const int epoll_events_types[] = {EPOLLIN, EPOLLOUT};
#ifdef DEBUG
	auto strrr = [](int x) {
		if (x == EPOLLIN) return "epollin";
		if (x == EPOLLOUT) return "epollout";
		if (x == EPOLLERR) return "epollerr";
		if (x == EPOLLHUP) return "epollhup";
		return "don't know";
	};
#endif
	
	for (int i = 0; i < amount; ++i) {
		for (auto x : epoll_events_types) {
			if (!invalid.count(events[i].data.fd) && (events[i].events & x)) {
				// if (!handlers.count(id(events[i].data.fd, x)))
						// assert(handlers.count(id(events[i].data.fd, x)));
				auto handler = handlers.at(id(events[i].data.fd, x));
				handler(events[i]);
			}
		}
	}

} 

void epoll_wrapper::invalidate(int fd) {
	invalid.insert(fd);
}


epoll_wrapper::id::id(int fd, int flag) : fd(fd), flags(flag) {}
epoll_wrapper::id::id(const epoll_event& d) : fd(d.data.fd), flags(d.events) {}

bool operator==(const epoll_wrapper::id& lhs, const epoll_wrapper::id& rhs) {
	return lhs.fd == rhs.fd && lhs.flags == rhs.flags;
}
bool operator<(const epoll_wrapper::id& lhs, const epoll_wrapper::id& rhs) {
	return lhs.fd < rhs.fd ||
			 (lhs.fd == rhs.fd && lhs.flags < rhs.flags);
}


