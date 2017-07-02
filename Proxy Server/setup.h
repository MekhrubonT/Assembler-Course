#ifndef SETUP_H
#define SETUP_H

// #define DEBUG
#define TIMER

#include <string>
#include <iostream>

const int READ_SIZE = 1000;
const std::string CRLF = "\r\n";
const std::string DCRLF = CRLF + CRLF;
const int NOT_GET_REQUEST = 2;
const int CANT_RESOLVE_HOST = 3;
const int NO_HOST_IN_REQUEST = 4;

namespace {
	inline void log() {
	}	

	template<typename T, typename... Args>
	void log(T first, Args... args) {
#ifdef DEBUG
		std::cout << first << " ";
		log(args...);
#endif

	}
}

#endif 