#define TIMER

#include "proxy_server.h"

#include <iostream>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <cassert>
#include <unistd.h>


#include <signal.h>
#include <fstream>


using namespace std;

#define AUTOLOCK(lock) unique_lock<mutex> autolock(lock)

const std::string BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\nServer: proxy\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 164\r\nConnection: close\r\n\r\n<html>\r\n<head><title>400 Bad Request</title></head>\r\n<body bgcolor=\"white\">\r\n<center><h1>400 Bad Request</h1></center>\r\n<hr><center>proxy</center>\r\n</body>\r\n</html>";


namespace {
    socket_wrapper listenning_socket(int port) {
        sockaddr_in serv_addr;
        socket_wrapper sock(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        
        if (sock.get_fd() < 0) {
            std::cerr << "Can't open socket";
            exit(1);
        }

        int temp = 1;
        if (setsockopt(sock.get_fd(), SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(temp)) < 0) {
            std::cerr << "setsockopt(SO_REUSEADDR) failed";
            exit(1);
        }
        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if (bind(sock.get_fd(), reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
            std::cerr << "Can't bind serv_addr";
            exit(1);
        }
        // std::cout << "listenning_socket is created: " << sock.get_fd() << "\n";
        listen(sock.get_fd(), 5);
        return sock;
    }


    socket_wrapper connection_socket(sockaddr addr) {
        socket_wrapper sock(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (connect(sock.get_fd(), &addr, sizeof(addr)) == -1 && errno != EINPROGRESS) {
            return socket_wrapper(-1);
        }
        return sock;
    }
}

proxy_server::proxy_server(int port) : port(port), socket(listenning_socket(port)), epoll(EPOLL_CLOEXEC),
				resolver_pool(10) {

    // std::cout << "Server created\n";


    epoll.add_event(socket.get_fd(), EPOLLIN, 
        [this](const epoll_event&) {
            connect_client();
        });

    int pipe_fds[2];
    if (pipe2(pipe_fds, O_NONBLOCK) == -1) {
        std::cout << "Can't create pipe\n";
        exit(10);
    }
    resolve_fd = std::move(socket_wrapper(pipe_fds[0]));
    notify_fd = std::move(socket_wrapper(pipe_fds[1]));
    epoll.add_event(resolve_fd.get_fd(), EPOLLIN, [this](const epoll_event&) {
        host_resolved();
    });
}

proxy_server::~proxy_server() {}

void proxy_server::notifier(int client) {
	// std::cout << "notifier called " << client << "\n";
	AUTOLOCK(resolved_queue_lock);
	resolved.push(client); 
    notify_fd.write("0");
}

void proxy_server::run() {
    while (true) {
        epoll.execute();
    }
}

void proxy_server::connect_client() {
    // std::cout << "client connected\n";
    client* cl = new client(socket.accept().release());
    timer_fd* timer = new timer_fd();

    // std::cout << "Client " << cl->get_fd() << "\n";

    int fd = cl->get_fd();

    // std::cout << "Client connected\t" << fd << " with timer " << timer->get_fd() << "\n";
    clients[fd] = std::move(std::unique_ptr<client>(cl));
    timers[fd] = std::shared_ptr<timer_fd>(timer);

    reset_timer(fd);
    epoll.add_event(fd, EPOLLIN, [this](const epoll_event& event) {
        read_from_client(event);
    });
#ifdef TIMER
    epoll.add_event(timer->get_fd(), EPOLLIN, [this, fd](const epoll_event&) {
        disconnect_client(fd);
    });
#endif
}

void proxy_server::disconnect_client(int fd) {
    // std::cout << "Disconnecting client " << fd << "\n";
    assert(clients.count(fd));
    client* cli = clients.at(fd).get();

    {   
        auto it = timers.find(cli->get_fd());
        assert(it != timers.end());
        if (it != timers.end()) {
            epoll.del_event(it->second->get_fd(), EPOLLIN);
            epoll.invalidate(it->second->get_fd());
            timers.erase(it);
        }
    }

    {
        auto it = requests.find(cli->get_fd());
        if (it != requests.end()) {
            requests.erase(cli->get_fd());
        }
    }

    if (cli->has_server()) {
        disconnect_server(cli->get_ser_fd());
    }

    epoll.del_event(cli->get_fd(), EPOLLIN);        
    epoll.del_event(cli->get_fd(), EPOLLOUT); 
    epoll.invalidate(cli->get_fd());
    clients.erase(cli->get_fd());
    // std::cout << "Clients deleted\n";
}

void proxy_server::disconnect_server(int fd) {
    // std::cout << "Disconnecting server\n";
    client* serv = clients.at(fd).get();
    client* cli = clients.at(serv->get_ser_fd()).get();
    cli->unbind();

    epoll.del_event(fd, EPOLLIN);        
    epoll.del_event(fd, EPOLLOUT); 
    epoll.invalidate(fd);
    clients.erase(fd);
}



void proxy_server::reset_timer(int fd) {
    timers.at(fd)->reset();
}

void proxy_server::read_from_client(const epoll_event& event) {
    // std::cout << "reading from client " << event.data.fd << "\n";
    auto client = clients.at(event.data.fd).get();
    reset_timer(client->get_fd());
    if (client->read(8192) == 0) {
        disconnect_client(event.data.fd);
    } else if (http_request::is_request_ready(client->get_data())) {
        // std::cout << client->get_data() << "\n";
        http_request* req = new (std::nothrow) http_request(client->get_data());
        if (req && !req->get_error()) {
            requests[client->get_fd()] = shared_ptr<http_request>(req);
            resolver_pool.submit(requests[client->get_fd()], bind(&proxy_server::notifier, this, client->get_fd()));  
            client->set_data(req->get_request());
        } else {
            disconnect_client(event.data.fd);
        }
        // epoll.del_event(event);
    }  
    // std::cout << "Gone\n";
}

void proxy_server::host_resolved() {
    // std::cout << "resolved\n";
    vector<int> temp;
    { 
        AUTOLOCK(resolved_queue_lock);
        int kol = resolve_fd.read(100).size();
        while (kol--) {
            temp.push_back(resolved.front());
            resolved.pop();
        }
    }
    for (auto x : temp) {
        if (requests.count(x)) {
            http_request* req = requests.at(x).get();
            // std::cout << "request prepared " << req->get_error() << "\n";
            client* cli = clients.at(x).get();
            client* server = new client(connection_socket(req->get_server()).release());
            if (req->get_error() || server->get_fd() == -1) {
                disconnect_client(cli->get_fd());
            } else {
                // std::cout << "Server " << server->get_fd() << "\n";
                if (cli->has_server()) {
                    disconnect_server(cli->get_ser_fd());
                }
                clients[server->get_fd()] = unique_ptr<client>(server);
                timers[server->get_fd()] = timers[cli->get_fd()];
                cli->bind(server);
                epoll.add_event(server->get_fd(), EPOLLOUT, 
                    [this](const epoll_event& event) {
                        write_to_server(event);
                });
            }
        }
    }
}

void proxy_server::write_to_server(const epoll_event& event) {
    // std::cout << "writting to server " << event.data.fd << "\n";
    client* server = clients.at(event.data.fd).get();
    reset_timer(server->get_fd());
    if (server->write() == -1) {
    	auto cli = clients.at(server->get_ser_fd()).get();
    	cli->set_data(BAD_REQUEST);
    	disconnect_server(server->get_fd());
    	epoll.add_event(cli->get_fd(), EPOLLOUT, 
    		[this](const epoll_event& event) {
    			write_to_client(event);
    		});
    } else if (server->get_data().empty()) {
        epoll.del_event(event);
        epoll.add_event(event.data.fd, EPOLLIN,
                [this](const epoll_event& event) {
                    read_from_server(event);
                });
    }
}

void proxy_server::read_from_server(const epoll_event& event) {
    // std::cout << "Reading from server\n";
    client* server = clients.at(event.data.fd).get();
    reset_timer(server->get_fd());

	// std::cout << server->get_data() << "\n";
    int pos = server->read(8192);
    if (pos == -1) {
    	server->set_data(BAD_REQUEST);
    }
    if (pos == 0 || http_request::is_response_finished(server->get_data())) {
	    epoll.del_event(event);
        epoll.del_event(server->get_ser_fd(), EPOLLIN);
        epoll.add_event(server->get_ser_fd(), EPOLLOUT, 
            [this](const epoll_event& event) {
                write_to_client(event);
            });

    }
}

void proxy_server::write_to_client(const epoll_event& event) {
    // std::cout << "Writting to client\n";
    client* cli = clients.at(event.data.fd).get();
    reset_timer(cli->get_fd());
    // std::cout << cli->get_data() << "\n";
    // std::cout << cli->get_data().size() << "\t";
    if (cli->write() == -1) {
    	disconnect_client(cli->get_fd());
    } else if (cli->get_data().empty()) {
        disconnect_client(cli->get_fd());
    }
    // std::cout << "\n\n\n";
}

int main(int argc, char **argv) {
	if (argc < 2) {
        cerr << "Port number required as argument\n";
        exit(1);
	}

	signal(SIGPIPE, SIG_IGN);

	if (signal(SIGINT, [](int signo) {
			if(signo == SIGINT) {
				std::cout << "SIGINT\n";
				exit(0);
			}
		}
		) == SIG_ERR) {
		std::cout << "Can't handle signals\n";
		exit(0);
	}

    proxy_server server(atoi(argv[1]));
    server.run();
    return 0;
}   

    