#include <iostream>
#include <sys/epoll.h>
#include <string>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <array>
#include <errno.h>
#include <fcntl.h>
#include <cassert>
#include <unordered_map>
#include <algorithm>

void error(std::string msg, int error_code) {
	std::cerr << msg << "\n";
	exit(1);
}	

int portno = 8002;
const int EPOLL_MAX_EVENTS_NUMBER = 10;
int evfd;

epoll_event build_client_event_object(int clientfd, int flags) {
	epoll_event client_event;
	client_event.data.fd = clientfd;
	client_event.events = flags;
	return client_event;
}

void epoll_ctl_helper(int fd, int op, int epoll_flags) {
    epoll_event client_event = build_client_event_object(fd, epoll_flags);
    epoll_ctl(evfd, op, fd, &client_event);
    if (epoll_flags == EPOLL_CTL_DEL) {
        close(fd);
    }
}

namespace non_blocking_write {
    int set_non_blocking(int fd)
    {
        int flags;

        if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
            flags = 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    struct echo_write {
        echo_write(int _fd) : fd(_fd), data(0), 
                                        sent(0), current_state(EPOLLIN) {
            set_non_blocking(fd);
        }
        
        bool handle() {
            if (sent == data) {
                sent = 0;
                data = read(fd, buf, BUFFERSIZE);
                if (data == 0 || (data == -1 && errno == EBADF)) {
                    epoll_ctl_helper(fd, EPOLL_CTL_DEL, EPOLLIN);
                    return true;
                }
                if (data == -1) {
                    checkerror();
                    data = 0;
                }
            }
            if (data != 0) {
                int writen = write(fd, buf + sent, data - sent);
                if (writen != -1) {
                    sent += writen;
                }
                if (sent == data && current_state != EPOLLIN) {
                    epoll_ctl_helper(fd, EPOLL_CTL_MOD, EPOLLIN);
                } else if (sent != data && current_state != EPOLLOUT) {
                    epoll_ctl_helper(fd, EPOLL_CTL_MOD, EPOLLOUT);
                }
            }
            return false;
        }

private: 
        void checkerror() {
            static const size_t not_allowed_errors_amount = 4;
            static const int not_allowed_errors[] 
            = {EAGAIN, EFAULT, EINVAL, EISDIR}; 
            static const std::string name[] 
            = {"EAGAIN", "EFAULT", "EINVAL", "EISDIR"};
            auto pos = std::find(not_allowed_errors, 
                not_allowed_errors + not_allowed_errors_amount, errno) 
            - not_allowed_errors;

            if (pos != not_allowed_errors_amount) {
                error("Not allowed error happened " + name[pos], 2);
            } 
        }

        const static int BUFFERSIZE = 2048;
        char buf[BUFFERSIZE];
        int fd;
        int data;
        int sent;
        int current_state;
    };
}

void client_handle(int clientfd) {
    using non_blocking_write::echo_write;
    static std::unordered_map<int, echo_write> clients;
    auto it = clients.find(clientfd);
    if (it == clients.end()) {
        it = clients.insert({clientfd, echo_write(clientfd)}).first;
    }

    if (it->second.handle()) {
        clients.erase(it);
    }

}
    
void socket_builder(int &socketfd, sockaddr_in &serv_addr) {
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		error("Can't open socket", 1);
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

void my_epoll_create(int &socketfd) {
    if ((evfd = epoll_create(EPOLL_MAX_EVENTS_NUMBER)) == -1) {
        error("Can't create epoll", 2);
    }
    epoll_ctl_helper(socketfd, EPOLL_CTL_ADD, EPOLLIN);
}

void add_client_to_epoll(int &socketfd, sockaddr_in& cli_addr, socklen_t socklen) {
    int clientfd = accept(socketfd, reinterpret_cast<sockaddr*>(&cli_addr), &socklen);
    if (clientfd >= 0) {
        epoll_ctl_helper(clientfd, EPOLL_CTL_ADD, EPOLLIN);
    }
}

int main(int argc, char **argv) {
    portno = atoi(argv[1]);

    int socketfd;
    sockaddr_in serv_addr, cli_addr;
    socklen_t socklen = sizeof(cli_addr);
 
    socket_builder(socketfd, serv_addr);
	socket_bind(socketfd, serv_addr);
    listen(socketfd, 5);
    my_epoll_create(socketfd);
 
    while (true) {
        static epoll_event events[EPOLL_MAX_EVENTS_NUMBER];
        size_t event_amount = epoll_wait(evfd, events, EPOLL_MAX_EVENTS_NUMBER, -1);
		
		for (size_t i = 0; i < event_amount; ++i) {
			if (events[i].data.fd == socketfd) {
				add_client_to_epoll(socketfd, cli_addr, socklen);
			} else {
				client_handle(events[i].data.fd);				
			}
		}        
    }

    close(evfd);
    close(socketfd);
    return 0;
}
