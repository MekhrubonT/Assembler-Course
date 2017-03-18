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

void error(std::string msg, int error_code) {
	std::cerr << msg << "\n";
	exit(1);
}	

const int portno = 8002;
const int EPOLL_MAX_EVENTS_NUMBER = 10;


void client_handle(int clientfd) {
	const static int BUFFERSIZE = 2048;
    int buf[BUFFERSIZE];
    int data = 0;
    data = read(clientfd, buf, BUFFERSIZE);
    int lo = 0;
    while (lo < data) {
        int writen = write(clientfd, buf + lo, data - lo);
        if (writen >= 0) {
            lo += writen;
        }
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

void my_epoll_create(int &evfd, int &socketfd, epoll_event &ev) {
    evfd = epoll_create(EPOLL_MAX_EVENTS_NUMBER);
    if (evfd == -1) {
        error("Can't create epoll", 2);
    }
    ev.events = EPOLLIN;
    ev.data.fd = socketfd;
    if (epoll_ctl(evfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        error("Can't listen to server: epoll", 2);
    }
}

void add_client_to_epoll(int &socketfd, int &evfd, sockaddr_in& cli_addr, socklen_t socklen) {
	int clientfd = accept(socketfd, reinterpret_cast<sockaddr*>(&cli_addr), &socklen);
	if (clientfd < 0) {
		return;
	}
	epoll_event client_event;
	client_event.data.fd = clientfd;
	client_event.events = EPOLLIN;
    if (epoll_ctl(evfd, EPOLL_CTL_ADD, clientfd, &client_event) == -1) {
        error("Can't listen to client: epoll", 2);
    }            
}

int main() {
    int socketfd, evfd;
    sockaddr_in serv_addr, cli_addr;
    socklen_t socklen = sizeof(cli_addr);
	epoll_event ev;
 

    socket_builder(socketfd, serv_addr);
	socket_bind(socketfd, serv_addr);
    listen(socketfd, 5);
    my_epoll_create(evfd, socketfd, ev);

    while (true) {
        static epoll_event events[EPOLL_MAX_EVENTS_NUMBER];
        size_t event_amount = epoll_wait(evfd, events, EPOLL_MAX_EVENTS_NUMBER, -1);
		
		for (size_t i = 0; i < event_amount; ++i) {
			if (events[i].data.fd == socketfd) {
				add_client_to_epoll(socketfd, evfd, cli_addr, socklen);
			} else {
				client_handle(events[i].data.fd);				
			}
		}        
    }

    close(socketfd);
    return 0;
}
//open 127.0.0.1 8001
