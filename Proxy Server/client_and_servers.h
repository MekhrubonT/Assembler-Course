#ifndef CLIENTANDSERVERS
#define CLIENTANDSERVERS

#include <sys/epoll.h>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <cstring>
#include <memory>

struct file_desc
{
	file_desc(int fd);
	file_desc();

	~file_desc();
	file_desc(const file_desc& other) = delete;
	file_desc& operator=(const file_desc& other) = delete;

	file_desc(file_desc&& other);
	file_desc& operator=(file_desc&& other);

	int release();
	int get_fd();
protected:
	int fd;

};

struct socket_wrapper : file_desc
{
	socket_wrapper();
	socket_wrapper(int);
	socket_wrapper(int, int, int);
	socket_wrapper accept();
	std::string read(size_t buffer_size);
	int write(const std::string&);	
};

// default 5 sec
struct timer_fd : file_desc
{
	timer_fd();
	void reset(long ns = 1ll * 70 * 100 * 1000 * 1000);
};

struct client
{
	client(int);

	client(const client& other) = delete;
	client& operator=(const client& other) = delete;

	int get_fd();
	int get_ser_fd();

	std::string get_data();
	void set_data(const std::string& data);

	bool has_server();
	
	void bind(client* ser);
	void unbind();
	void set_message(const std::string& msg);


	int read(size_t buf);
	int write();

private:
	socket_wrapper socket;
	std::string data;
	client* ser;
};

#endif