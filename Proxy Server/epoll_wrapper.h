#ifndef EPOLL_WRAPPER_H
#define EPOLL_WRAPPER_H

#include "client_and_servers.h"

#include <sys/epoll.h>
#include <map>
#include <set>
#include <functional>

struct epoll_wrapper : file_desc
{
	epoll_wrapper();
	epoll_wrapper(int);
	~epoll_wrapper();

	epoll_wrapper(const epoll_wrapper&) = delete;
	epoll_wrapper& operator=(const epoll_wrapper&) const = delete;


	epoll_wrapper& build(int flags);
	void add_event(int fd, int flags, std::function<void(epoll_event&)> handle);
	void del_event(int fd, int flags);
	void del_event(const epoll_event& event);

	void execute();
	void invalidate(int fd);

private:
	int change_client(int fd, int op, int flags);

	struct id {
		id(int fd, int flags);
		id(const epoll_event& event);

		friend bool operator==(const id& lhs, const id& rhs);
		friend bool operator<(const id& lhs, const id& rhs);
//	private: 
		int fd, flags;
	};
	friend bool operator==(const id& lhs, const id& rhs);
	friend bool operator<(const id& lhs, const id& rhs);

	std::map<id, std::function<void(epoll_event&)>> handlers;
	std::set<int> invalid;
};


#endif