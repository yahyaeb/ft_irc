
#include "../resources/Irc.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>


// int main(int argc, char **argv)
// {
// 	if (argc != 3) {
// 		std::cerr << "Usage: ./ircserv <port> <password>\n";
// 		return 1;
// 	}

// 	int port = atoi(argv[1]);
// 	std::string password = argv[2];

// 	if (port < 1024 || port > 65535) {
// 		std::cerr << "Invalid port number.\n";
// 		return 1;
// 	}
// 	if (password.empty()) {
// 		std::cerr << "Password cannot be empty.\n";
// 		return 1;
// 	}

// 	std::cout << "Port: " << port << std::endl;
// 	std::cout << "Password: " << password << std::endl;

// }

static bool isValidPort(const char* s) {
    for (size_t i = 0; s[i]; ++i)
		if (!std::isdigit(s[i])) return false;
    long p = std::strtol(s, NULL, 10);
    return p >= 1024 && p <= 65535;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port 1024-65535> <password>\n";
        return 1;
    }
    if (!isValidPort(argv[1])) {
        std::cerr << "Invalid port\n";
        return 1;
    }
    int port = std::atoi(argv[1]);
    std::string password = argv[2];

    try {
        Server srv(port, password);
        srv.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
