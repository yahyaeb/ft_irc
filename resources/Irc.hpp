#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <poll.h>
#include <cerrno>

struct Client
{
    int         fd;
    std::string inbuf;  
    bool        registered;
    std::string nick;
    std::string user;
    Client(): fd(-1), registered(false) {}
};

class Server
{
	private:
		int                             listen_fd;
		int                             port;
		std::string                     password;
		std::vector<struct pollfd>      pollfds;
		std::map<int, Client>           clients;

		void initSocket();
		void setNonBlocking(int fd);
		void acceptNewClients();
		void handleClientReadable(size_t idx);
		void closeClient(int fd);
		void processLines(Client &c);
		void handleLine(Client &c, const std::string &line);
		void sendRaw(int fd, const std::string &msg);

	public:
		Server(int port, const std::string &pass);
		~Server();
		void run();
};

#endif
