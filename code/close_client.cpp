/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   close_client.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yel-bouk <yel-bouk@student.42nice.fr>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/18 18:17:26 by yel-bouk          #+#    #+#             */
/*   Updated: 2025/10/18 18:49:38 by yel-bouk         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../resources/Irc.hpp"

void Server::closeClient(int fd)
{
	if (!clients.count(fd))
	{
		for (size_t i = 0; i < pollfds.size(); ++i)
			if (pollfds[i].fd == fd)
			{
				pollfds.erase(pollfds.begin() + i); break;
			}
		close(fd);
		return;
	}

	Client c = clients[fd]; // copy for nick after erase
	std::cout << "[-] Client fd=" << fd << " disconnected" << std::endl;
	// Remove nick mapping
	if (!c.nick.empty())
	{
		std::map<std::string,int>::iterator itn = nick_to_fd.find(c.nick);
		if (itn != nick_to_fd.end() && itn->second == fd) nick_to_fd.erase(itn);
	}

	// Remove from channels;
	for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); )
	{
		Channel &ch = it->second;
		bool wasMember = ch.members.erase(fd) > 0;
		ch.operators.erase(fd);

		if (wasMember) {
			std::string prefix = ":" + (c.nick.empty()? std::string("*"): c.nick);
			std::string quitmsg = prefix + " QUIT :Client Quit\r\n";
			for (std::set<int>::const_iterator m = ch.members.begin(); m != ch.members.end(); ++m)
				sendRaw(*m, quitmsg);

			if (ch.operators.empty() && !ch.members.empty()) {
				int newop = *ch.members.begin(); // simple policy
				ch.operators.insert(newop);
				const Client &nc = clients[newop];
				std::string wire = ":" + (nc.nick.empty()? std::string("*"): nc.nick)
								   + " MODE " + ch.name + " +o " + (nc.nick.empty()? std::string("*") : nc.nick) + "\r\n";
				for (std::set<int>::const_iterator m = ch.members.begin(); m != ch.members.end(); ++m)
					sendRaw(*m, wire);
			}
		}

		if (ch.members.empty())
		{
			channels.erase(it++);
			continue;
		}
		else
			++it;
	}
	for (size_t i = 0; i < pollfds.size(); ++i)
		if (pollfds[i].fd == fd)
		{
			pollfds.erase(pollfds.begin() + i);
			break;
		}

	close(fd);
	clients.erase(fd);
}