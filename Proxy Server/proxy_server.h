#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include "epoll_wrapper.h"
#include "client_and_servers.h"
#include "Executors/fixed_thread_pool.h"
#include <mutex>
#include <queue>
#include <unordered_map>

struct proxy_server
{
	proxy_server(int port);
	~proxy_server();

    proxy_server(const proxy_server&) = delete;
    proxy_server& operator=(const proxy_server&) = delete;

	void run();

private:
	void connect_client();
	void disconnect_client(int fd);
	void disconnect_server(int fd);
	
	void read_from_client(const epoll_event& event);
	void host_resolved();
	void write_to_server(const epoll_event& event);
	void read_from_server(const epoll_event& event);
	void write_to_client(const epoll_event& event);

	void reset_timer(int fd);

	void notifier(int client_fd);
	int port;

// Proxy server socket on given port
    socket_wrapper socket;
// Proxy server epoll on all descriptors
    epoll_wrapper epoll;

// resolver pool
    fixed_thread_pool resolver_pool;

// 
    std::unordered_map<int, std::unique_ptr<client>> clients;
    std::unordered_map<int, std::shared_ptr<http_request>> requests;
    std::unordered_map<int, std::shared_ptr<timer_fd>> timers;


// resolved http clients
	std::queue<int> resolved;
	std::mutex resolved_queue_lock;

// pipeline notifier
	socket_wrapper notify_fd;
	socket_wrapper resolve_fd;
};



#endif