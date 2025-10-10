#ifndef IRC_HPP
#define IRC_HPP

#include <iostream>
#include <string>
#include <map>


class Server {
private:
	int listen_fd;
	int port;
	std::string password;

};

#endif