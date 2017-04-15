// TODO 
// EINTR 
// Error handling: accept(ECONN ABORTED, EMFILE, ENFILE), send(EPIPE)
// Можно отключить генерацию сигнала SIGPIPE, тогда EPIPE <=> close(fd)

#include <iostream>
#include <sys/epoll.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <cassert>
#include <unordered_map>
#include <algorithm>
#include <queue>
#include <memory>
#include <signal.h>

void error(const std::string& msg, int error_code);
void epoll_ctl_helper(const int evfd, const int fd, const int op, const int epoll_flags);


namespace non_blocking_write {
    struct echo_write {
        echo_write(const int _evfd, const int _fd) : evfd(_evfd), fd(_fd), data(0), 
                                        sent(0), current_state(EPOLLIN), 
                                        last_run(time(0)), 
                                        is_fin(false) {
        }
        
        int get_fd() {
            return fd;
        }
        bool is_finished() {
            return is_fin;
        }

        time_t get_last_run() {
            return last_run;
        }

        ~echo_write() {
            epoll_ctl_helper(evfd, fd, EPOLL_CTL_DEL, current_state);
        }


        bool handle() {
            if (is_finished()) {
                std::cerr << "Finished writer should not be called anymore\n";
                return true;
            }

            last_run = time(0);
            if (sent == data) {
                sent = 0;
                data = read(fd, buf, BUFFERSIZE);
                if (data == 0 || (data == -1 && errno == EBADF)) {
                    return is_fin = true;
                }
                if (data == -1) {
                    data = 0;
                }
            }
            if (data != 0) {
                int writen = write(fd, buf + sent, data - sent);
                if (writen != -1) {
                    sent += writen;
                } else if (errno == EPIPE) {
                	is_fin = true;
                	data = sent = 0;
                	return true;
                }
                if (sent == data && current_state != EPOLLIN) {
                    epoll_ctl_helper(evfd, fd, EPOLL_CTL_MOD, EPOLLIN);
                } else if (sent != data && current_state != EPOLLOUT) {
                    epoll_ctl_helper(evfd, fd, EPOLL_CTL_MOD, EPOLLOUT);
                }
            }
            return false;
        }

    private: 

        const static int BUFFERSIZE = 2048;
        char buf[BUFFERSIZE];
        const int fd, evfd;
        int data;
        int sent;
        int current_state;
        time_t last_run;
        bool is_fin;
    };
}

typedef std::shared_ptr<non_blocking_write::echo_write> shwr;

const int EPOLL_MAX_EVENTS_NUMBER = 10;
const time_t TIMEOUT_CONSTANT = 5; // Seconds

static std::queue<std::pair<time_t, shwr>> q;
static std::unordered_map<int, shwr> clients;

void error(const std::string& msg, int error_code) {
	std::cerr << msg << "\n";
	exit(1);
}	

epoll_event build_client_event_object(const int clientfd, const int flags) {
	epoll_event client_event;
	client_event.data.fd = clientfd;
	client_event.events = flags;
	return client_event;
}

void epoll_ctl_helper(const int evfd, const int fd, const int op, const int epoll_flags) {
    epoll_event client_event = build_client_event_object(fd, epoll_flags);
    epoll_ctl(evfd, op, fd, &client_event);
    if (op == EPOLL_CTL_DEL) {
        std::cerr << fd << " should be closed\n";
        close(fd);
    }
}

void client_handle(const int clientfd) {
    std::cerr << "Client want to interact: " << clientfd << "\n";
    auto it = clients.find(clientfd);
    assert (it != clients.end());
    if (it->second->handle()) {
        clients.erase(it);
    }
}

void waiter_deleter() {
    while (!q.empty() && q.front().first <= time(0)) {
        auto x = q.front();
        q.pop();

        if (x.second->is_finished()) {
        } else if (x.second->get_last_run() > x.first - TIMEOUT_CONSTANT) {
            x.first = time(0) + TIMEOUT_CONSTANT;
            q.push(x);
        } else {
            std::cerr << "Timeout: " << x.second->get_fd() << "\n";   
            std::cerr << x.first << " " << time(0) << "\n";

            std::cout << "owners amount" << x.second.use_count() << "\n";
            clients.erase(x.second->get_fd());
            std::cout << "owners amount" << x.second.use_count() << "\n";
            x.second.reset();
        }
    }
}
    
void socket_builder(int &socketfd, sockaddr_in &serv_addr, const int portno) {
	if ((socketfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) < 0) {
		error("Can't open socket", 1);
	}
	int temp = 1;
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(temp)) < 0) {
	    error("setsockopt(SO_REUSEADDR) failed", 2);
	}


    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);	
}

void socket_bind(const int socketfd, sockaddr_in &serv_addr) {
    if (bind(socketfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        error("Can't bind serv_addr", 2); 
    }
}

void my_epoll_create(int &evfd, const int socketfd) {
    if ((evfd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
        error("Can't create epoll", 2);
    }
    epoll_ctl_helper(evfd, socketfd, EPOLL_CTL_ADD, EPOLLIN);
}

void add_client_to_epoll(const int evfd, const int socketfd, sockaddr_in& cli_addr, socklen_t socklen) {
    int clientfd = accept4(socketfd, reinterpret_cast<sockaddr*>(&cli_addr), &socklen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (clientfd >= 0) {
        epoll_ctl_helper(evfd, clientfd, EPOLL_CTL_ADD, EPOLLIN);

        std::cerr << "Client added\t" << clientfd << "\n";
        shwr client_ptr = std::make_shared<non_blocking_write::echo_write>(evfd, clientfd);
        clients.insert({clientfd, client_ptr});
        q.push({time(0) + TIMEOUT_CONSTANT, client_ptr});
        std::cerr << time(0) + TIMEOUT_CONSTANT << "\n";
        std::cerr << "\t owners count " << client_ptr.use_count() << "\n";
    }
}


int main(int argc, char **argv) {
	if (argc < 2) {
		error("Port number required as argument", 1);
	}
    int portno = atoi(argv[1]);
    int socketfd;
	int evfd;

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
    sockaddr_in serv_addr, cli_addr;
    socklen_t socklen = sizeof(cli_addr);
 
    socket_builder(socketfd, serv_addr, portno);
	socket_bind(socketfd, serv_addr);
    listen(socketfd, 5);
    my_epoll_create(evfd, socketfd);

    std::cerr << "Timeuot is " << TIMEOUT_CONSTANT << " seconds\n";
    while (true) {
        static epoll_event events[EPOLL_MAX_EVENTS_NUMBER];
        
        std::cerr << "Epoll wait start " <<  time(0)  << "\n";
        std::cerr << "\t waiting for " << (q.empty() ? -1 : std::max(0l, q.front().first - time(0))) << "\n";
        size_t event_amount = epoll_wait(evfd, events, EPOLL_MAX_EVENTS_NUMBER, 
            q.empty() ? -1 : std::max(0l, (q.front().first - time(0)) * 1000));
        std::cerr << "End " << time(0) << "\n";


		for (size_t i = 0; i < event_amount; ++i) {
			if (events[i].data.fd == socketfd) {
				add_client_to_epoll(evfd, socketfd, cli_addr, socklen);
			} else {
				client_handle(events[i].data.fd);				
			}
		}        

        waiter_deleter();
    }

    return 0;
}

