#include "../resources/Irc.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>



Server::Server(int p, const std::string &pass): listen_fd(-1), port(p), password(pass) {
	initSocket();
}

Server::~Server() {
	for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it)
		close(it->first);
	if (listen_fd != -1)
		close(listen_fd);
}

void Server::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL,O_NONBLOCK) failed");
}

void Server::initSocket() {
	// init socket
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1)
		throw std::runtime_error("socket() failed");

	// Enables to use already-used ports in case socket restarts
	int tru = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(tru)) == -1)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

	// fucntion to change the default socket blocking to non-locking
	setNonBlocking(listen_fd);

	// set sockaddr variable and bind the socket to all available nets
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	// bind socket with addr to rep an IP and Port
	if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		throw std::runtime_error("bind() failed");

	//setting up a listeing queue for the socket
	if (listen(listen_fd, 128) == -1)
		throw std::runtime_error("listen() failed");

	// setup pollfd for monitoring
	struct pollfd p;
	p.fd = listen_fd;
	p.events = POLLIN;
	p.revents = 0;
	pollfds.push_back(p);

	std::cout << "[OK] Listening on port " << port << std::endl;
}

void Server::acceptNewClients() {
	while (true) {
		// accepts the first connection on the queue and gives us a new socket
		int cfd = accept(listen_fd, NULL, NULL);
		if (cfd == -1) 
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			perror("accept");
			break;
		}
		// need to set the new socket to non-blocking
		try 
		{
			setNonBlocking(cfd);
		}
		catch (...)
		{ 
			close(cfd); continue;
		}
		// setup p for monitoring and pushing the socket to pollfds
		struct pollfd p;
		p.fd = cfd;
		p.events = POLLIN;
		p.revents = 0;
		pollfds.push_back(p);

		//creating a clinet instance and init the fd creating 
		Client c;
		c.fd = cfd;
		// Adding the client to the clients container
		clients[cfd] = c;

		std::cout << "[+] Client connected fd=" << cfd << std::endl;
		sendRaw(cfd, ":ft_irc NOTICE * :Welcome! Please PASS/NICK/USER\r\n");
	}
}

void Server::closeClient(int fd)
{
	std::cout << "[-] Client fd=" << fd << " disconnected" << std::endl;
	close(fd);
	clients.erase(fd);
	for (std::vector<struct pollfd>::iterator it = pollfds.begin(); it != pollfds.end(); ++it) {
		if (it->fd == fd) 
		{
			pollfds.erase(it); break;
		}
	}
}

void Server::handleClientReadable(size_t idx)
{

	int fd = pollfds[idx].fd;
	char buf[4096];
	// recv is the equivelant of read() but specific to sockets
	ssize_t n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
			closeClient(fd);
		return;
	}
	// we access existing sockets (by reference)
	Client &c = clients[fd];
	// we append received bytes to our Client struct buff
	c.inbuf.append(buf, n);
	if (c.inbuf.size() > 8192)
	{
		sendRaw(fd, "ERROR :Input too long\r\n");
		closeClient(fd);
		return;
	}
	processLines(c);
}

void Server::processLines(Client &c) {
	std::string::size_type pos;
	while ((pos = c.inbuf.find("\r\n")) != std::string::npos)
	{
		std::string line = c.inbuf.substr(0, pos);
		c.inbuf.erase(0, pos + 2);
		if (line.size() > 510)
		{
			sendRaw(c.fd, "ERROR :Line too long\r\n");
			closeClient(c.fd);
			return;
		}
		handleLine(c, line);
	}
}

static std::string upper(const std::string &s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); ++i) r[i] = std::toupper(r[i]);
	return r;
}

void Server::handleLine(Client &c, const std::string &line) {
	// tokenizer
	// Params after ':' is the trailing part 
	std::string cmd;
	std::string rest;
	std::string::size_type sp = line.find(' ');

	if (sp == std::string::npos)
	{ 
		cmd = line;
	}
	else
	{
		cmd = line.substr(0, sp); rest = line.substr(sp + 1);
	}

	cmd = upper(cmd);

	bool reg_cmd = (cmd=="PING"||cmd=="PASS"||cmd=="NICK"||cmd=="USER"||cmd=="QUIT");
	if (!c.registered && !reg_cmd)
	{
		sendRaw(c.fd, ":ft_irc 451 * :You have not registered\r\n");
		return;
	}
	if (cmd == "PING")
	{
		sendRaw(c.fd, "PONG :ft_irc\r\n");
		return;
	}
	else if (cmd == "PASS") {
		// Check server password
		if (rest == password || (rest.size() > 0 && rest[0] == ':' && rest.substr(1) == password)) 
		{
			// if it passes
			 c.pass_ok = true;
			sendRaw(c.fd, ":ft_irc NOTICE * :PASS accepted\r\n");
		}
		else
		{
			//else wrong pass
			sendRaw(c.fd, "ERROR :Bad password\r\n");
			closeClient(c.fd);
		}
		return;
	}
	else if (cmd == "NICK")
	{
		std::string nick = rest.erase(rest.find_last_not_of(" \n\r\t") + 1);
		if (nick.empty())
		{
			sendRaw(c.fd, ":ft_irc 431 * :No NICKNAME given\r\n");
			return;
		}
		if (!c.nick.empty())
			nick_to_fd.erase(c.nick);
		c.nick = rest;
		nick_to_fd[c.nick] = c.fd;
		sendRaw(c.fd, ":ft_irc NOTICE * :NICK set\r\n");
		return;
	}
	else if (cmd == "USER")
	{
		std::string user = rest.erase(rest.find_last_not_of(" \n\r\t") + 1);
		if (user.empty())
		{
			sendRaw(c.fd, ":ft_irc 431 * :No USER given\r\n");
			return;
		}
		c.user = rest;
		sendRaw(c.fd, ":ft_irc NOTICE * :USER set\r\n");
		// If PASS/NICK/USER are all present, send welcome numerics && set c.registered=true
		if (!c.registered && c.pass_ok && !c.nick.empty() && !c.user.empty())
		{
			c.registered = true;
			sendRaw(c.fd, ":ft_irc 001 " + c.nick + " :Welcome to ft_irc!\r\n");
			return;
		}
	}
	else if (cmd == "QUIT")
	{
		closeClient(c.fd);
		return;
	}
	else if (cmd == "JOIN")
	{
		if (rest.empty())
		{
			sendRaw(c.fd, ":ft_irc 461 * JOIN :Not enough parameters\r\n");
				return;
		}
		std::string chan = rest;
		if (!chan.empty() && chan[0]==':')
			chan.erase(0,1);
		while (!chan.empty() && chan[0] == ' ')
			chan.erase(0, 1);
		while (!chan.empty() && chan[chan.size() - 1] == ' ')
			chan.erase(chan.size() - 1, 1);

		if (chan.empty() || chan[0] != '#')
		{
			sendRaw(c.fd, ":ft_irc 479 * " + chan + " :Illegal channel name\r\n");
			return;
		}
		Channel &cobj = channels[chan];
		if (cobj.name.empty()) cobj.name = chan;
		cobj.members.insert(c.fd);

		std::string nickToUse = c.nick;
		if (nickToUse.empty())
			nickToUse = "*";

		sendRaw(c.fd, ":" + nickToUse + " JOIN " + chan + "\r\n");


		broadcastToChannel(chan, c.fd, ":" + c.nick + " JOIN " + chan + "\r\n");
		return;
	}
	else if (cmd == "PRIVMSG")
	{
		// PRIVMSG "target" :"text"
		if (rest.empty()) { 
			sendRaw(c.fd, ":ft_irc 461 * PRIVMSG :Not enough parameters\r\n"); 
			return; 
		}

		// split target and trailing
		std::string::size_type sp2 = rest.find(' ');
		std::string target = rest;
		if (sp2 != std::string::npos)
			target = rest.substr(0, sp2);

		std::string text;
		if (sp2 != std::string::npos) {
			std::string after = rest.substr(sp2 + 1);
			text = after;
			if (!after.empty() && after[0] == ':')
				text = after.substr(1);
		}

		if (target.empty() || text.empty()) {
			sendRaw(c.fd, ":ft_irc 461 * PRIVMSG :Not enough parameters\r\n");
			return;
		}

		std::string wire = ":";
		if (c.nick.empty())
			wire += "*";
		else
			wire += c.nick;
		wire += " PRIVMSG " + target + " :" + text + "\r\n";

		if (!target.empty() && target[0] == '#') {
			// Channel message
			std::map<std::string, Channel>::iterator it = channels.find(target);
			if (it == channels.end()) { 
				sendRaw(c.fd, ":ft_irc 403 * " + target + " :No such channel\r\n"); 
				return; 
			}
			// check sender is a member
			if (it->second.members.find(c.fd) == it->second.members.end()) {
				sendRaw(c.fd, ":ft_irc 404 * " + target + " :Cannot send to channel\r\n");
				return;
			}
			// Broadcast to all except sender
			for (std::set<int>::const_iterator m = it->second.members.begin(); m != it->second.members.end(); ++m)
			{
				if (*m == c.fd) continue;
				sendRaw(*m, wire);
			}
			return;
		} else {
			// Direct message to nick
			std::map<std::string,int>::iterator nit = nick_to_fd.find(target);
			if (nit == nick_to_fd.end())
			{ 
				sendRaw(c.fd, ":ft_irc 401 * " + target + " :No such nick\r\n"); 
				return; 
			}
			sendRaw(nit->second, wire);
			return;
		}
	}
	else
		sendRaw(c.fd, ":ft_irc NOTICE * :You said: " + line + "\r\n");
}

void Server::sendRaw(int fd, const std::string &msg) {
	ssize_t bytes_sent = 0;
	while (bytes_sent < (ssize_t)msg.size())
	{
		ssize_t n = send(fd, msg.c_str() + bytes_sent, msg.size() - bytes_sent, 0);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) break;
			closeClient(fd);
			return;
		}
		bytes_sent += n;
	}
}

void Server::run() {
	while (true) {
		int ret = poll(&pollfds[0], pollfds.size(), 1000);
		if (ret < 0) {
			if (errno == EINTR) continue;
			perror("poll");
			break;
		}
		for (size_t i = 0; i < pollfds.size(); ++i) {
			if (pollfds[i].revents & POLLIN) {
				if (pollfds[i].fd == listen_fd) acceptNewClients();
				else handleClientReadable(i);
			}
			if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				if (pollfds[i].fd != listen_fd) closeClient(pollfds[i].fd);
			}
		}
	}
}
