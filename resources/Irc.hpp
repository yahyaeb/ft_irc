#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <poll.h>
#include <cerrno>
#include <set>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <sstream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

struct Client
{
	int	fd;
	std::string inbuf;
	std::string outbuf;
	bool	registered;
	std::string nick;
	std::string user;
	bool	pass_ok;

	Client(): fd(-1), registered(false), pass_ok(false) {}
};

struct Channel {
	std::string name;
	int max_members;
	std::set<int> members;
	std::set<int> operators;           // fds of ops
	std::string topic;
	std::set<char> modes;              // {'i','t','k','l'} as flags;
	std::string key;                   // MODE +k value
	std::set<int> invited;
	time_t created;                    // when first createdhandleClientRead
	std::vector<int> join_order;       // order of joins
	Channel(): max_members(0), created(std::time(NULL)) {}
};

class Server
{
private:
	int	listen_fd;
	int	port;
	std::string password;
	std::vector<struct pollfd> pollfds;
	std::map<int, Client>	clients;
	std::map<std::string,int>	nick_to_fd;

	void initSocket();
	void setNonBlocking(int fd);
	void acceptNewClients();
	void handleClientReadable(size_t idx);
	void handleClientWritable(size_t idx);	// flush outbuf on POLLOUT
	void closeClient(int fd);
	void processLines(Client &c);
	void handleLine(Client &c, const std::string &line);
	void sendRaw(int fd, const std::string &msg);
	void maybeRegister(Client &c);	// complete registration when ready

public:
	std::map<std::string, Channel> channels;
	Server(int port, const std::string &pass);
	~Server();
	void run();

	void addChannelMode(Channel &chan, char mode);
	void removeChannelMode(Channel &chan, char mode);
	bool hasChannelMode(const Channel &chan, char mode);

	void cmdMode(Client &c, const std::string &chanName, const std::vector<std::string> &params);
	void cmdKick(Client &c, const std::string &chanName, const std::string &targetNick, const std::string &reason);
	void cmdInvite(Client &c, const std::string &targetNick, const std::string &chanName);
	void cmdTopic(Client &c, const std::string &channel_name, const std::string &topic);



	void broadcastToChannel(const std::string& chan, int from_fd, const std::string& msg)
	{
		std::map<std::string, Channel>::iterator it = channels.find(chan);
		if (it == channels.end())
			return;
		for (std::set<int>::const_iterator m = it->second.members.begin();
			 m != it->second.members.end(); ++m)
		{
			if (*m == from_fd)
				continue;
			sendRaw(*m, msg);
		}
	}
};

std::string upper(const std::string &s);
std::string trim(const std::string& s);
std::vector<std::string> splitWords(const std::string& s);
void closeClient(int fd);


#endif


