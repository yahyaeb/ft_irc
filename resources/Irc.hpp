#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <poll.h>
#include <cerrno>
#include <set>

struct Client
{
	int		fd;
	std::string inbuf;  
	bool	registered;
	std::string nick;
	std::string user;
	bool	pass_ok;
	Client(): fd(-1), registered(false) , pass_ok(false){}
};

struct Channel
{
	std::string name;
	std::set<int> members;
};

class Server
{
	private:
		int	 listen_fd;
		int	port;
		std::string	password;
		std::vector<struct pollfd>	pollfds;
		std::map<int, Client>	clients;

		void initSocket();
		void setNonBlocking(int fd);
		void acceptNewClients();
		void handleClientReadable(size_t idx);
		void closeClient(int fd);
		void processLines(Client &c);
		void handleLine(Client &c, const std::string &line);
		void sendRaw(int fd, const std::string &msg);
		std::map<std::string,int> nick_to_fd;


	public:
		std::map<std::string, Channel> channels;
		Server(int port, const std::string &pass);
		~Server();
		void run();

		
void broadcastToChannel(const std::string& chan, int from_fd, const std::string& msg)
{
	std::map<std::string, Channel>::iterator it = channels.find(chan);
	if (it == channels.end()) return;
	for (std::set<int>::const_iterator m = it->second.members.begin();
		m != it->second.members.end(); ++m) {
		if (*m == from_fd) continue;
		sendRaw(*m, msg);
	}
}
};




#endif
