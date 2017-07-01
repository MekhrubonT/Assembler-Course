cmake_minimum_required(VERSION 3.0)
project(debugger)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pthread -g")

include_directories()


set(SOURCE_FILES
		client_and_servers.cpp 
 		epoll_wrapper.cpp 

	    Executors/fixed_thread_pool.cpp 
	    Executors/single_thread_pool.cpp

	    http_request.cpp 
	    server.cpp 
	    )

add_executable(server ${SOURCE_FILES})
