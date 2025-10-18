#include "../resources/Irc.hpp"

template<typename T>
static std::string to_string98(T v)
{
	std::ostringstream oss;
	oss << v;
	return oss.str();
}

Server::Server(int p, const std::string &pass): listen_fd(-1), port(p), password(pass)
{
	initSocket();
}

Server::~Server()
{
	for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it)
		close(it->first);
	if (listen_fd != -1)
		close(listen_fd);
}

void Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL,O_NONBLOCK) failed");
}

void Server::initSocket()
{
	// Avoid SIGPIPE killing process on send
	std::signal(SIGPIPE, SIG_IGN);

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1)
		throw std::runtime_error("socket() failed");

	int tru = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(tru)) == -1)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

	setNonBlocking(listen_fd);

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		throw std::runtime_error("bind() failed");

	if (listen(listen_fd, 128) == -1)
		throw std::runtime_error("listen() failed");

	struct pollfd p;
	p.fd = listen_fd;
	p.events = POLLIN;
	p.revents = 0;
	pollfds.push_back(p);

	std::cout << "[OK] Listening on port " << port << std::endl;
}

void Server::acceptNewClients()
{
	while (true)
	{
		int cfd = accept(listen_fd, NULL, NULL);
		if (cfd == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			perror("accept");
			break;
		}

		try
		{
			setNonBlocking(cfd);
		}
		catch (...)
		{
			close(cfd);
			continue;
		}

		struct pollfd p;
		p.fd = cfd;
		p.events = POLLIN;
		p.revents = 0;
		pollfds.push_back(p);

		Client c;
		c.fd = cfd;
		clients[cfd] = c;

		std::cout << "[+] Client connected fd=" << cfd << std::endl;
		sendRaw(cfd, ":ft_irc NOTICE * :Welcome! Please PASS/NICK/USER\r\n");
	}
}


void Server::handleClientReadable(size_t idx)
{
	int fd = pollfds[idx].fd;
	char buf[4096];
	ssize_t n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
			closeClient(fd);
		return;
	}

	Client &c = clients[fd];
	c.inbuf.append(buf, n);
	if (c.inbuf.size() > 8192)
	{
		sendRaw(fd, "ERROR :Input too long\r\n");
		closeClient(fd);
		return;
	}
	processLines(c);
}

void Server::processLines(Client &c)
{
	while (true)
	{
		std::string::size_type pos = c.inbuf.find('\n');
		if (pos == std::string::npos) break;
		std::string line = c.inbuf.substr(0, pos);
		c.inbuf.erase(0, pos + 1);
		if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);

		if (line.size() > 510)
		{
			sendRaw(c.fd, "ERROR :Line too long\r\n");
			closeClient(c.fd);
			return;
		}
		if (!line.empty())
			handleLine(c, line);
	}
}

void Server::maybeRegister(Client &c)
{
	if (!c.registered && c.pass_ok && !c.nick.empty() && !c.user.empty())
	{
		c.registered = true;
		sendRaw(c.fd, ":ft_irc 001 " + c.nick + " :Welcome to ft_irc!\r\n");
	}
}

void Server::cmdMode(Client &c, const std::string &chanName, const std::vector<std::string> &params)
{
	std::map<std::string, Channel>::iterator it = channels.find(chanName);
	if (it == channels.end())
	{
		sendRaw(c.fd, ":ft_irc 403 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :No such channel\r\n");
		return;
	}
	Channel &ch = it->second;

	if (params.empty())
	{
		std::string modes = "+";
		std::string args;
		if (ch.modes.count('i')) modes += "i";
		if (ch.modes.count('t')) modes += "t";
		if (ch.modes.count('k'))
		{
			modes += "k";
			args += " *";
		}
		if (ch.modes.count('l'))
		{
			modes += "l";
			args += " " + (ch.max_members > 0 ? to_string98(ch.max_members) : std::string("0"));
		}
		sendRaw(c.fd, ":ft_irc 324 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " " + modes + (args.empty()? "" : " " + trim(args)) + "\r\n");
		return;
	}

	if (ch.members.find(c.fd) == ch.members.end())
	{
		sendRaw(c.fd, ":ft_irc 442 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :You're not on that channel\r\n");
		return;
	}
	if (!ch.operators.count(c.fd))
	{
		sendRaw(c.fd, ":ft_irc 482 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :You're not channel operator\r\n");
		return;
	}

	if (params[0].empty() || (params[0][0] != '+' && params[0][0] != '-'))
	{
		sendRaw(c.fd, ":ft_irc 472 " + (c.nick.empty()?std::string("*"):c.nick) + " :Unknown mode flag\r\n");
		return;
	}

	bool adding = true;
	const std::string &modestr = params[0];
	size_t pidx = 1;

	std::string appliedFlags;
	std::vector<std::string> appliedArgs;

	for (size_t i = 0; i < modestr.size(); ++i)
	{
		char chmode = modestr[i];
		if (chmode == '+')
		{
			adding = true;
			continue;
		}
		if (chmode == '-')
		{
			adding = false;
			continue;
		}

		switch (chmode)
		{
			case 'i': {
				if (adding) ch.modes.insert('i'); else ch.modes.erase('i');
				appliedFlags.push_back(adding? '+' : '-'); appliedFlags.push_back('i');
				break;
			}
			case 't': {
				if (adding) ch.modes.insert('t'); else ch.modes.erase('t');
				appliedFlags.push_back(adding? '+' : '-'); appliedFlags.push_back('t');
				break;
			}
			case 'k': {
				if (adding)
				{
					if (pidx >= params.size() || params[pidx].empty())
					{
						sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " MODE :Not enough parameters\r\n");
						return;
					}
					ch.modes.insert('k'); ch.key = params[pidx++];
					appliedFlags += "+k"; appliedArgs.push_back(ch.key);
				} else {
					ch.modes.erase('k'); ch.key.clear();
					appliedFlags += "-k";
				}
				break;
			}
			case 'l': {
				if (adding)
				{
					if (pidx >= params.size())
					{
						sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " MODE :Not enough parameters\r\n");
						return;
					}
					const std::string &lim = params[pidx++];
					char *endp = 0; long v = strtol(lim.c_str(), &endp, 10);
					if (endp == lim.c_str() || v <= 0 || v > 1000000)
					{
						sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " MODE :Invalid limit\r\n");
						return;
					}
					ch.modes.insert('l'); ch.max_members = static_cast<int>(v);
					appliedFlags += "+l"; appliedArgs.push_back(to_string98(v));
				} else {
					ch.modes.erase('l'); ch.max_members = 0;
					appliedFlags += "-l";
				}
				break;
			}
			case 'o': {
				if (pidx >= params.size() || params[pidx].empty())
				{
					sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " MODE :Not enough parameters\r\n");
					return;
				}
				std::string nick = params[pidx++];
				std::map<std::string,int>::iterator nit = nick_to_fd.find(nick);
				if (nit == nick_to_fd.end())
				{
					sendRaw(c.fd, ":ft_irc 401 " + (c.nick.empty()?std::string("*"):c.nick) + " " + nick + " :No such nick\r\n");
					return;
				}
				int targetFd = nit->second;
				if (!ch.members.count(targetFd))
				{
					sendRaw(c.fd, ":ft_irc 441 " + (c.nick.empty()?std::string("*"):c.nick) + " " + nick + " " + chanName + " :They aren't on that channel\r\n");
					return;
				}
				if (adding)
					ch.operators.insert(targetFd);
				else
					ch.operators.erase(targetFd);
				appliedFlags.push_back(adding? '+' : '-'); appliedFlags.push_back('o');
				appliedArgs.push_back(nick);
				break;
			}
			default: {
				sendRaw(c.fd, ":ft_irc 472 " + (c.nick.empty()?std::string("*"):c.nick) + " " + std::string(1,chmode) + " :is unknown mode char\r\n");
				break;
			}
		}
	}
	if (!appliedFlags.empty())
	{
		std::string prefix = ":" + (c.nick.empty()?std::string("*"):c.nick);
		std::string wire = prefix + " MODE " + chanName + " " + appliedFlags;
		for (size_t i = 0; i < appliedArgs.size(); ++i) wire += " " + appliedArgs[i];
		wire += "\r\n";
		for (std::set<int>::const_iterator m = ch.members.begin(); m != ch.members.end(); ++m)
			sendRaw(*m, wire);
	}
}

void Server::cmdKick(Client &c, const std::string &chanName, const std::string &targetNick, const std::string &reason)
{
	// Find channel
	std::map<std::string, Channel>::iterator chit = channels.find(chanName);
	if (chit == channels.end())
	{
		sendRaw(c.fd, ":ft_irc 403 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :No such channel\r\n");
		return;
	}
	Channel &ch = chit->second;

	// Must be on the channel
	if (!ch.members.count(c.fd))
	{
		sendRaw(c.fd, ":ft_irc 442 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :You're not on that channel\r\n");
		return;
	}

	// Must be operator
	if (!ch.operators.count(c.fd))
	{
		sendRaw(c.fd, ":ft_irc 482 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :You're not channel operator\r\n");
		return;
	}

	// Resolve target nick
	std::map<std::string,int>::iterator nit = nick_to_fd.find(targetNick);
	if (nit == nick_to_fd.end())
	{
		sendRaw(c.fd, ":ft_irc 401 " + (c.nick.empty()?std::string("*"):c.nick) + " " + targetNick + " :No such nick\r\n");
		return;
	}
	int targetFd = nit->second;

	// Target must be on the channel
	if (!ch.members.count(targetFd))
	{
		sendRaw(c.fd, ":ft_irc 441 " + (c.nick.empty()?std::string("*"):c.nick) + " " + targetNick + " " + chanName + " :They aren't on that channel\r\n");
		return;
	}

	// Build the wire message
	const std::string kicker = (c.nick.empty()? std::string("*") : c.nick);
	std::string wire = ":" + kicker + " KICK " + chanName + " " + targetNick + " :" + (reason.empty() ? std::string("Kicked") : reason) + "\r\n";

	// Send to all current members (including the target)
	for (std::set<int>::const_iterator m = ch.members.begin(); m != ch.members.end(); ++m)
		sendRaw(*m, wire);
	// Also make sure the kicked client gets it (in case they aren’t iterated due to timing)
	sendRaw(targetFd, wire);

	// Remove target from channel
	ch.members.erase(targetFd);
	ch.operators.erase(targetFd);

	// If channel empty, delete it
	if (ch.members.empty())
	{
		channels.erase(chit);
		return;
	}

	// If no operators remain, auto-promote someone (your policy)
	if (ch.operators.empty())
	{
		int newop = *ch.members.begin(); // or use join_order
		ch.operators.insert(newop);
		const Client &nc = clients[newop];
		std::string modeWire = ":" + (nc.nick.empty()? std::string("*"): nc.nick)
							 + " MODE " + chanName + " +o " + (nc.nick.empty()? std::string("*"): nc.nick) + "\r\n";
		for (std::set<int>::const_iterator m = ch.members.begin(); m != ch.members.end(); ++m)
			sendRaw(*m, modeWire);
	}
}

void Server::handleLine(Client &c, const std::string &line)
{
	std::string cmd, rest;
	std::string::size_type sp = line.find(' ');
	if (sp == std::string::npos)
		cmd = line;
	else
	{
		cmd = line.substr(0, sp);
		rest = line.substr(sp + 1);
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
		std::string token = trim(rest);
		if (!token.empty() && token[0]==':') token.erase(0,1);
		if (token.empty()) token = "ft_irc";
		sendRaw(c.fd, "PONG :" + token + "\r\n");
		return;
	}
	else if (cmd == "PASS")
	{
		std::string pw = rest;
		if (!pw.empty() && pw[0]==':') pw.erase(0,1);
		if (pw == password)
		{
			c.pass_ok = true;
			sendRaw(c.fd, ":ft_irc NOTICE * :PASS accepted\r\n");
			maybeRegister(c);
		}
		else
		{
			sendRaw(c.fd, "ERROR :Bad password\r\n");
			closeClient(c.fd);
		}
		return;
	}
	else if (cmd == "MODE")
	{
		if (rest.empty())
		{
			sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " MODE :Not enough parameters\r\n");
			return;
		}
		std::vector<std::string> toks = splitWords(rest);
		std::string target = toks.empty() ? "" : toks[0];
		if (target.empty() || target[0] != '#')
		{
			sendRaw(c.fd, ":ft_irc 403 " + (c.nick.empty()?std::string("*"):c.nick) + " " + target + " :No such channel\r\n");
			return;
		}
		std::vector<std::string> params;
		for (size_t i = 1; i < toks.size(); ++i) params.push_back(toks[i]);
		cmdMode(c, target, params);
		return;
	}
	else if (cmd == "NICK")
	{
		std::string nick = trim(rest);
		if (nick.empty())
		{
			sendRaw(c.fd, ":ft_irc 431 * :No nickname given\r\n");
			return;
		}
		std::map<std::string,int>::iterator itn = nick_to_fd.find(nick);
		if (itn != nick_to_fd.end() && itn->second != c.fd)
		{
			sendRaw(c.fd, ":ft_irc 433 * " + nick + " :Nickname is already in use\r\n");
			return;
		}
		if (!c.nick.empty()) nick_to_fd.erase(c.nick);
		c.nick = nick;
		nick_to_fd[c.nick] = c.fd;
		sendRaw(c.fd, ":ft_irc NOTICE * :NICK set\r\n");
		maybeRegister(c);
		return;
	}
	else if (cmd == "USER")
	{
		std::string user = trim(rest);
		if (user.empty())
		{
			sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " USER :Not enough parameters\r\n");
			return;
		}
		c.user = user;
		sendRaw(c.fd, ":ft_irc NOTICE * :USER set\r\n");
		maybeRegister(c);
		return;
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
		std::string arg = rest;
		std::string chanName, providedKey;
		std::string::size_type sp2 = arg.find(' ');
		chanName = (sp2==std::string::npos) ? arg : arg.substr(0, sp2);
		if (sp2 != std::string::npos)
		{
			providedKey = arg.substr(sp2+1);
			while (!providedKey.empty() && providedKey[0]==' ') providedKey.erase(0,1);
			if (!providedKey.empty() && providedKey[0]==':') providedKey.erase(0,1);
		}
		while (!chanName.empty() && chanName[0]==':') chanName.erase(0,1);
		chanName = trim(chanName);

		if (chanName.empty() || chanName[0] != '#')
		{
			sendRaw(c.fd, ":ft_irc 479 * " + chanName + " :Illegal channel name\r\n");
			return;
		}
		bool newlyCreated = (channels.find(chanName) == channels.end());
		Channel &ch = channels[chanName];
		if (newlyCreated)
		{
			ch.name = chanName;
			ch.created = std::time(NULL);
		}

		if (ch.members.count(c.fd))
		{
			sendRaw(c.fd, ":ft_irc 443 " + c.nick + " " + chanName + " :is already on channel\r\n");
			return;
		}

		if (ch.modes.count('i') && !ch.invited.count(c.fd))
		{
			sendRaw(c.fd, ":ft_irc 473 " + c.nick + " " + chanName + " :Invite-only channel\r\n");
			return;
		}
		if (ch.modes.count('k') && ch.key != providedKey)
		{
			sendRaw(c.fd, ":ft_irc 475 " + c.nick + " " + chanName + " :Bad channel key\r\n");
			return;
		}
		if (ch.modes.count('l') && ch.max_members > 0 && (int)ch.members.size() >= ch.max_members)
		{
			sendRaw(c.fd, ":ft_irc 471 " + c.nick + " " + chanName + " :Channel is full\r\n");
			return;
		}

		ch.members.insert(c.fd);
		ch.join_order.push_back(c.fd);
		if (newlyCreated) ch.operators.insert(c.fd);
		ch.invited.erase(c.fd);

		std::string prefix = ":" + (c.nick.empty() ? std::string("*") : c.nick);
		sendRaw(c.fd, prefix + " JOIN " + chanName + "\r\n");
		broadcastToChannel(chanName, c.fd, prefix + " JOIN " + chanName + "\r\n");

		if (ch.topic.empty())
			sendRaw(c.fd, ":ft_irc 331 " + c.nick + " " + chanName + " :No topic is set\r\n");
		else
			sendRaw(c.fd, ":ft_irc 332 " + c.nick + " " + chanName + " :" + ch.topic + "\r\n");

		std::string names;
		for (std::set<int>::const_iterator it = ch.members.begin(); it != ch.members.end(); ++it)
		{
			const Client &mc = clients[*it];
			bool isOp = ch.operators.count(*it) != 0;
			if (!names.empty()) names += " ";
			names += (isOp ? "@" : "") + (mc.nick.empty()? std::string("*") : mc.nick);
		}
		sendRaw(c.fd, ":ft_irc 353 " + c.nick + " = " + chanName + " :" + names + "\r\n");
		sendRaw(c.fd, ":ft_irc 366 " + c.nick + " " + chanName + " :End of NAMES list\r\n");
		return;
	}
	else if (cmd == "KICK")
	{
		// KICK #chan nick [:reason...]
		if (rest.empty())
		{
			sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " KICK :Not enough parameters\r\n");
			return;
		}
		std::vector<std::string> toks = splitWords(rest);
		if (toks.size() < 2) 
		{
			sendRaw(c.fd, ":ft_irc 461 " + (c.nick.empty()?std::string("*"):c.nick) + " KICK :Not enough parameters\r\n");
			return;
		}
		const std::string chanName  = toks[0];
		const std::string targetNick= toks[1];
		std::string reason = (toks.size() >= 3) ? toks[2] : std::string("Kicked");

		if (chanName.empty() || chanName[0] != '#')
		{
			sendRaw(c.fd, ":ft_irc 403 " + (c.nick.empty()?std::string("*"):c.nick) + " " + chanName + " :No such channel\r\n");
			return;
		}
		cmdKick(c, chanName, targetNick, reason);
		return;
	}
	else if (cmd == "PRIVMSG")
	{
		if (rest.empty())
		{
			sendRaw(c.fd, ":ft_irc 411 * :No recipient given (PRIVMSG)\r\n");
			return;
		}
		std::string::size_type sp2 = rest.find(' ');
		std::string target = (sp2 == std::string::npos) ? rest : rest.substr(0, sp2);
		std::string text;
		if (sp2 != std::string::npos)
		{
			std::string after = rest.substr(sp2 + 1);
			text = after;
			if (!after.empty() && after[0] == ':')
				text = after.substr(1);
		}
		target = trim(target);
		text = trim(text);
		if (target.empty()) 
		{
			sendRaw(c.fd, ":ft_irc 411 * :No recipient given (PRIVMSG)\r\n");
			return;
		}
		if (text.empty())
		{
			sendRaw(c.fd, ":ft_irc 412 * :No text to send\r\n");
			return;
		}
		std::string wire = ":" + (c.nick.empty()? std::string("*"): c.nick) + " PRIVMSG " + target + " :" + text + "\r\n";
		if (!target.empty() && target[0] == '#')
		{
			std::map<std::string, Channel>::iterator it = channels.find(target);
			if (it == channels.end())
			{
				sendRaw(c.fd, ":ft_irc 403 * " + target + " :No such channel\r\n");
				return;
			}
			if (it->second.members.find(c.fd) == it->second.members.end()) 
			{
				sendRaw(c.fd, ":ft_irc 404 * " + target + " :Cannot send to channel\r\n");
				return;
			}
			for (std::set<int>::const_iterator m = it->second.members.begin(); m != it->second.members.end(); ++m)
			{
				if (*m == c.fd) continue;
				sendRaw(*m, wire);
			}
			return;
		}
		else
		{
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
	else if (cmd == "TOPIC")
	{
		std::string channel_name;
		std::string topic;
		std::string::size_type space_pos = rest.find(' ');
		if (space_pos != std::string::npos)
		{
			channel_name = rest.substr(0, space_pos);
			topic = rest.substr(space_pos + 1);
		}
		else
			channel_name = rest;
		cmdTopic(c, channel_name, topic);
		return;
	}
	else
		sendRaw(c.fd, ":ft_irc NOTICE * :You said: " + line + "\r\n");
}

void Server::sendRaw(int fd, const std::string &msg)
{
	if (!clients.count(fd))
		return;
	Client &c = clients[fd];
	c.outbuf += msg;

	for (size_t i = 0; i < pollfds.size(); ++i)
	{
		if (pollfds[i].fd == fd)
		{
			pollfds[i].events |= POLLOUT;
			break;
		}
	}
}

void Server::handleClientWritable(size_t idx)
{
	int fd = pollfds[idx].fd;
	if (!clients.count(fd))
		return;
	Client &c = clients[fd];
	while (!c.outbuf.empty())
	{
		ssize_t n = send(fd, c.outbuf.data(), c.outbuf.size(), 0);
		if (n > 0)
			c.outbuf.erase(0, n);
		else
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			closeClient(fd);
			return;
		}
	}
		if (c.outbuf.empty())
		{
			pollfds[idx].events &= ~POLLOUT;
		}
}

void Server::run()
{
	while (true)
	{
		int ret = poll(&pollfds[0], pollfds.size(), 1000);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}
		for (ssize_t i = (ssize_t)pollfds.size() - 1; i >= 0; --i)
		{
			if ((size_t)i >= pollfds.size())
				continue;

			if (pollfds[i].revents & POLLIN)
			{
				if (pollfds[i].fd == listen_fd) acceptNewClients();
				else handleClientReadable((size_t)i);
			}
			if ((size_t)i < pollfds.size() && (pollfds[i].revents & POLLOUT))
				handleClientWritable((size_t)i);
			if ((size_t)i < pollfds.size() && (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))) 
			{
				if (pollfds[i].fd != listen_fd)
					closeClient(pollfds[i].fd);
			}
		}
	}
}

void Server::addChannelMode(Channel &chan, char mode)
{
	chan.modes.insert(mode);
}

void Server::removeChannelMode(Channel &chan, char mode)
{
	chan.modes.erase(mode);
}

bool Server::hasChannelMode(const Channel &chan, char mode)
{
	return chan.modes.find(mode) != chan.modes.end();
}

void Server::cmdTopic(Client &c, const std::string &channel_name, const std::string &topic)
{
	std::map<std::string, Channel>::iterator it = channels.find(channel_name);
	if (it == channels.end())
	{
		sendRaw(c.fd, ":ft_irc 403 " + c.nick + " " + channel_name + " :No such channel\r\n");
		return;
	}
	Channel &chan = it->second;

	if (chan.members.find(c.fd) == chan.members.end())
	{
		sendRaw(c.fd, ":ft_irc 442 " + c.nick + " " + channel_name + " :You're not on that channel\r\n");
		return;
	}

	if (!topic.empty())
	{
		if (hasChannelMode(chan, 't') && chan.operators.find(c.fd) == chan.operators.end())
		{
			sendRaw(c.fd, ":ft_irc 482 " + c.nick + " " + channel_name + " :You're not channel operator\r\n");
			return;
		}
		chan.topic = topic;
		sendRaw(c.fd, ":ft_irc 332 " + c.nick + " " + channel_name + " :" + topic + "\r\n");
		broadcastToChannel(channel_name, c.fd, ":" + c.nick + " TOPIC " + channel_name + " :" + topic + "\r\n");
	}
	else
	{
		if (chan.topic.empty())
			sendRaw(c.fd, ":ft_irc 331 " + c.nick + " " + channel_name + " :No topic is set\r\n");
		else
			sendRaw(c.fd, ":ft_irc 332 " + c.nick + " " + channel_name + " :" + chan.topic + "\r\n");
	}
}
