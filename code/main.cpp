#include "../resources/Irc.hpp"
#include <stdlib.h>


int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cerr << "Usage: ./ircserv <port> <password>\n";
		return 1;
	}

	int port = atoi(argv[1]);
	std::string password = argv[2];

	if (port < 1024 || port > 65535) {
		std::cerr << "Invalid port number.\n";
		return 1;
	}
	if (password.empty()) {
		std::cerr << "Password cannot be empty.\n";
		return 1;
	}

	std::cout << "Port: " << port << std::endl;
	std::cout << "Password: " << password << std::endl;

}
