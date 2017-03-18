#include <bits/stdc++.h>
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

const int N = 2048;
const int portno = 8001;

void work_with_client(int clientfd) {
    int buf[N];
    int data = 0;
    while ((data = read(clientfd, buf, N))) {
        int lo = 0;
        while (lo < data) {
            int writen = write(clientfd, buf + lo, data - lo);
            if (writen >= 0) {
                lo += writen;
            }
        }
    }
    close(clientfd);
    std::cout << "client disconnected\n";
}

int main() {
    int socketfd;
    struct sockaddr_in serv_addr, cli_addr;

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		error("Can't open socket", 1);
	}

    bzero((char *) &serv_addr, sizeof(serv_addr));
    

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(socketfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        error("Can't bind serv_addr", 2); 
    }

    listen(socketfd, 5);
    socklen_t socklen = sizeof(cli_addr);
    std::vector<std::thread> v;
    while (true) {
        int clientfd;
        if ((clientfd = accept(socketfd, reinterpret_cast<sockaddr*>(&cli_addr), &socklen)) >= 0) {
            v.emplace_back(work_with_client, clientfd);
        }
    }
    while (!v.empty()) {
        v.back().join();
        v.pop_back();
    }
    close(socketfd);
    return 0;
}
//open 127.0.0.1 8001