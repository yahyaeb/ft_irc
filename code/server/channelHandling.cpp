#include "../../resources/Irc.hpp"

void Server::HandleJoinCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);

    if (!client)
        return;
    if (!client->isRegistered())
    {
        SendToClient(fd, ":server 451 * :You have not registered");
        return;
    }
    std::istringstream iss(args);
    std::string channelName, channelPassword;
    iss >> channelName >> channelPassword;

    if (channelName.empty() || channelName[0] != '#')
    {
        SendToClient(fd, ":server 403 " + client->getNickname() + " " + channelName + " :No such channel");
        return;
    };
    int count = std::count(channelName.begin(), channelName.end(), '#');
    if (count > 1)
        multipleChannels(fd, channelName, channelPassword);
    else
    {
        Channel *channel = GetChannelByName(channelName);

        if (!channel)
        {
            channel = CreateChannel(channelName, client);
            std::cout << "client: " << client->getUsername() << "created the: " << channel->getName() << " channel" << std::endl;
        }
        else
        {
            if (channel->isMember(fd))
            {
                SendToClient(fd, ":server 443 " + client->getNickname() + " " + channelName + " :is already on channel");
                return;
            }
            if (channel->isInviteOnly() && !channel->isInvited(fd))
            {
                SendToClient(fd, ":server 473 " + client->getNickname() + " " + channelName + " :Cannot join channel (+i)");
                return;
            }
            if (channel->hasPassword() && channel->getPassword() != channelPassword)
            {
                SendToClient(fd, ":server 475 " + client->getNickname() + " " + channelName + " :Cannot join channel (+k)");
                return;
            }
            if (channel->hasUserLimit() && channel->getClients().size() >= channel->getUserLimit())
            {
                SendToClient(fd, ":server 471 " + client->getNickname() + " " + channelName + " :Cannot join channel (+l)");
                return;
            }
            channel->addClient(client);
        }
        if (channel->isInvited(fd))
            channel->removeInvited(fd);
        std::string joinMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost JOIN " + channelName;
        channel->broadcastToChannel(joinMsg, -1);
        if (!channel->getTopic().empty())
        {
            SendToClient(fd, ":server 332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
        }
        else
        {
            SendToClient(fd, ":server 331 " + client->getNickname() + " " + channelName + " :No topic is set");
        }
        std::string userList = "353 " + client->getNickname() + " = " + channelName + " :";
        std::vector<Client *> clients = channel->getClients();
        for (size_t i = 0; i < clients.size(); i++)
        {
            if (channel->isOperator(clients[i]->getFd()))
                userList += "@";
            userList += clients[i]->getNickname();
            if (i < clients.size() - 1)
                userList += " ";
        }
        SendToClient(fd, ":server " + userList);
        SendToClient(fd, ":server 366 " + client->getNickname() + " " + channelName + " :End of /NAMES list");
        std::cout << "Client <" << fd << "> (" << client->getNickname() << ") joined " << channelName << std::endl;
    }
}

void Server::multipleChannels(int fd, std::string channelName, std::string channelPassword)
{
    std::vector<std::string> splitChannels = SplitChannels(channelName);
    std::vector<std::string> splitPasswords = SplitChannels(channelPassword);
    if (splitChannels.empty())
        throw(std::runtime_error("Error: could not split channels/passwords\n"));

    for (size_t i = 0; i < splitChannels.size(); i++)
    {
        std::string args = splitChannels[i];
        if (i < splitPasswords.size() && !splitPasswords[i].empty())
            args += " " + splitPasswords[i];
        HandleJoinCommand(fd, args);
    }
}

void Server::HandlePrivmsgCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isRegistered())
    {
        SendToClient(fd, ":server 451 * :You have not registered");
        return;
    }
    size_t space = args.find(' ');
    if (space == std::string::npos)
    {
        SendToClient(fd, "411 :No recipient given (PRIVMSG)");
        return;
    }
    std::string messageTarget = args.substr(0, space);
    std::string message = args.substr(space + 1);
    if (message.empty() || message[0] != ':')
    {
        SendToClient(fd, "412 :No text to send");
        return;
    }
    message = message.substr(1); // enlever les : avant le message
    if (messageTarget[0] == '#')
    {
        Channel *channel = GetChannelByName(messageTarget);
        if (!channel)
        {
            SendToClient(fd, "403" + messageTarget + ":No such channel");
            return;
        }
        if (!channel->isMember(fd))
        {
            SendToClient(fd, "404" + messageTarget + ":Cannot send to channel");
            return;
        }
        std::string fullMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost PRIVMSG " + messageTarget + " :" + message;
        channel->broadcastToChannel(fullMsg, fd);

        std::cout << "PRIVMSG to channel " << messageTarget << " from " << client->getNickname() << ": " << message << std::endl;
    }
    else
    {
        Client *targetClient = GetClientByNickname(messageTarget);

        if (!targetClient)
        {
            SendToClient(fd, "401 " + messageTarget + " :No such nick/channel");
            return;
        }
        std::string fullMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost PRIVMSG " + messageTarget + " :" + message;
        SendToClient(targetClient->getFd(), fullMsg);
        std::cout << "PRIVMSG from " << client->getNickname() << " to " << messageTarget << ": " << message << std::endl;
    }
}

void Server::HandleKickCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isRegistered())
    {
        SendToClient(fd, "451 :You have not registered");
        return;
    }

    std::istringstream iss(args);
    std::string channelName, targetToKick, reason;
    iss >> channelName >> targetToKick;
    std::getline(iss, reason);

    if (!reason.empty() && reason[0] == ' ')
        reason.substr(1);
    if (!reason.empty() && reason[0] == ':')
        reason.substr(1);
    if (reason.empty())
        reason = client->getNickname(); // raison de kick par defaul == nickname

    if (channelName.empty() || targetToKick.empty())
    {
        SendToClient(fd, "461 KICK :Not enough parameters");
        return;
    }
    Channel *channel = GetChannelByName(channelName);
    if (!channel)
    {
        SendToClient(fd, "403" + channelName + ":No such channel");
        return;
    }
    if (!channel->isMember(fd))
    {
        SendToClient(fd, "442" + channelName + ":You're not in that channel");
        return;
    }
    if (!channel->isOperator(fd))
    {
        SendToClient(fd, "482" + channelName + ":You're not channel operator");
        return;
    }
    Client *clientTokick = GetClientByNickname(targetToKick);
    if (!clientTokick)
    {
        SendToClient(fd, "401" + targetToKick + ":No such client/server");
        return;
    }
    if (!channel->isMember(clientTokick->getFd()))
    {
        SendToClient(fd, "441 " + targetToKick + " " + channelName + " :They aren't on that channel");
        return;
    }
    std::string kickMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost KICK " + channelName + " " + targetToKick + " :" + reason;
    channel->broadcastToChannel(kickMsg, -1);
    channel->removeClient(clientTokick->getFd());
    std::cout << targetToKick << " kicked from " << channelName << " by " << client->getNickname() << " (reason: " << reason << ")" << std::endl;
    RemoveChannelIfEmpty(channelName);
}

void Server::HandleInviteCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isRegistered())
    {
        SendToClient(fd, "451 :You have not registered");
        return;
    }
    std::istringstream iss(args);
    std::string targetNick, channelName;
    iss >> targetNick >> channelName;
    if (targetNick.empty() || channelName.empty())
    {
        SendToClient(fd, "461 INVITE :Not enough parameters");
        return;
    }
    Channel *channel = GetChannelByName(channelName);
    if (!channel)
    {
        SendToClient(fd, "403 " + channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(fd))
    {
        SendToClient(fd, "442 " + channelName + " :You're not on that channel");
        return;
    }
    if (channel->isInviteOnly() && !channel->isOperator(fd))
    {
        SendToClient(fd, "482 " + channelName + " :You're not channel operator");
        return;
    }
    Client *targetClient = GetClientByNickname(targetNick);
    if (!targetClient)
    {
        SendToClient(fd, "401 " + targetNick + " :No such nick/channel");
        return;
    }
    if (channel->isMember(targetClient->getFd()))
    {
        SendToClient(fd, "443 " + targetNick + " " + channelName + " :is already on channel");
        return;
    }
    channel->addInvited(targetClient->getFd());
    SendToClient(fd, "341 " + client->getNickname() + " " + targetNick + " " + channelName);
    std::string inviteMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost INVITE " + targetNick + " " + channelName;
    SendToClient(targetClient->getFd(), inviteMsg);
    std::cout << client->getNickname() << " invited " << targetNick << " to " << channelName << std::endl;
}

void Server::HandleTopicCommand(int fd, std::string args, std::string command)
{
    (void)command;
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    size_t space = args.find(' ');
    std::string channelName;
    std::string newTopic;
    if (space == std::string::npos)
        channelName = args;
    else
    {
        channelName = args.substr(0, space);
        newTopic = args.substr(space + 1);
        if (!newTopic.empty() && newTopic[0] == ':')
            newTopic.substr(1);
        if (newTopic[0] == ':' && newTopic[1] == ' ')
        {
            SendToClient(fd, "461" + client->getNickname() + ' ' + args + " :leading spaces are not allowed");
            return;
        }
    }
    if (channelName.empty())
    {
        SendToClient(fd, "461 TOPIC :Not enough parameters");
        return;
    }
    Channel *channel = GetChannelByName(channelName);
    if (!channel)
    {
        SendToClient(fd, "403 " + channelName + " :No such channel");
        return;
    }
    if (!channel->isMember(fd))
    {
        SendToClient(fd, "442 " + channelName + " :You're not on that channel");
        return;
    }
    if (newTopic.empty())
    {
        if (channel->getTopic().empty())
            SendToClient(fd, "331 " + client->getNickname() + " " + channelName + " :No topic is set");
        else
            SendToClient(fd, "332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
        return;
    }
    if (channel->isTopicRestricted() && !channel->isOperator(fd))
    {
        SendToClient(fd, "482 " + channelName + " :You're not channel operator");
        return;
    }
    channel->setTopic(newTopic);
    std::string topicMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost TOPIC " + channelName + " :" + newTopic;
    channel->broadcastToChannel(topicMsg, -1);
    std::cout << client->getNickname() << " changed topic of " << channelName << " to: " << newTopic << std::endl;
}

void Server::HandleModeCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isRegistered())
    {
        SendToClient(fd, "451 :You have not registered");
        return;
    }
    std::istringstream iss(args);
    std::string channelName, modeString, modeParam;
    iss >> channelName >> modeString >> modeParam;
    if (channelName.empty())
    {
        SendToClient(fd, "461 MODE :Not enough parameters");
        return;
    }
    Channel *channel = GetChannelByName(channelName);
    if (!channel)
    {
        SendToClient(fd, "403 " + channelName + " :No such channel");
        return;
    }
    if (modeString.empty())
    {
        std::string modes = "+";
        if (channel->isInviteOnly())
            modes += "i";
        if (channel->isTopicRestricted())
            modes += "t";
        if (channel->hasPassword())
            modes += "k";
        if (channel->hasUserLimit())
            modes += "l";
        SendToClient(fd, "324 " + client->getNickname() + " " + channelName + " " + modes);
        return;
    }
    if (!channel->isOperator(fd))
    {
        SendToClient(fd, "482 " + channelName + " :You're not channel operator");
        return;
    }
    bool adding = true;
    std::string appliedModes;
    std::string modeParams;

    for (size_t i = 0; i < modeString.length(); i++)
    {
        char mode = modeString[i];

        if (mode == '+')
        {
            adding = true;
            continue;
        }
        else if (mode == '-')
        {
            adding = false;
            continue;
        }
        if (mode == 'i')
        {
            channel->setInviteOnly(adding);
            if (adding)
                appliedModes += "+";
            else
                appliedModes += "-";
            appliedModes += "i";
        }
        else if (mode == 't')
        {
            channel->setTopicRestricted(adding);
            if (adding)
                appliedModes += "+";
            else
                appliedModes += "-";
            appliedModes += "t";
        }
        else if (mode == 'k')
        {
            if (adding)
            {
                if (modeParam.empty())
                {
                    SendToClient(fd, "461 MODE :Not enough parameters");
                    return;
                }
                channel->setPassword(modeParam);
                appliedModes += "+k";
                modeParams += " " + modeParam;
            }
            else
            {
                channel->setPassword("");
                appliedModes += "-k";
            }
        }
        else if (mode == 'o')
        {
            if (modeParam.empty())
            {
                SendToClient(fd, "461 MODE :Not enough parameters");
                return;
            }

            Client *targetClient = GetClientByNickname(modeParam);
            if (!targetClient)
            {
                SendToClient(fd, "401 " + modeParam + " :No such nick/channel");
                return;
            }

            if (!channel->isMember(targetClient->getFd()))
            {
                SendToClient(fd, "441 " + modeParam + " " + channelName + " :They aren't on that channel");
                return;
            }

            if (adding)
            {
                channel->addOperator(targetClient->getFd());
                appliedModes += "+";
            }
            else
            {
                channel->removeOperator(targetClient->getFd());
                appliedModes += "-";
            }
            appliedModes += "o";
            modeParams += " " + modeParam;
        }
        else if (mode == 'l')
        {
            if (adding)
            {
                if (modeParam.empty())
                {
                    SendToClient(fd, "461 MODE :Not enough parameters");
                    return;
                }

                int limit = atoi(modeParam.c_str());
                if (limit <= 0)
                {
                    SendToClient(fd, "696 " + channelName + " l :Invalid limit");
                    return;
                }

                channel->setUserLimit(limit);
                appliedModes += "+l";
                modeParams += " " + modeParam;
            }
            else
            {
                channel->removeUserLimit();
                appliedModes += "-l";
            }
        }
        else
        {
            std::string unknownMode;
            unknownMode += mode;
            SendToClient(fd, "472 " + unknownMode + " :is unknown mode char to me");
        }
    }
    if (!appliedModes.empty())
    {
        std::string modeMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost MODE " + channelName + " " + appliedModes + modeParams;
        channel->broadcastToChannel(modeMsg, -1);
    }
}