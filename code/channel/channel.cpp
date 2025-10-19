#include "../../resources/Irc.hpp"

Channel::Channel(std::string name)
{
    this->_channelName = name;
    this->_channelPassword = "";
    this->_channelTopic = "";
    this->_inviteOnly = false;
    this->_topicRestricted = false;
    this->_hasPassword = false;
    this->_hasUserLimit = false;
    this->_userLimit = 0;
    this->_botClient = NULL;
}
Channel::~Channel()
{
    if (_botClient)
        delete _botClient;
};
std::string Channel::getName() const { return _channelName; }
std::string Channel::getTopic() const { return _channelTopic; }
std::string Channel::getPassword() const { return _channelPassword; }
std::vector<Client *> Channel::getClients() const { return _clientsInChannel; }
size_t Channel::getUserLimit() const { return _userLimit; }
bool Channel::isInviteOnly() const { return _inviteOnly; }
bool Channel::isTopicRestricted() const { return _topicRestricted; }
bool Channel::hasPassword() const { return _hasPassword; }
bool Channel::hasUserLimit() const { return _hasUserLimit; }

bool Channel::isOperator(int fd) const
{
    return _operators.find(fd) != _operators.end();
}

bool Channel::isInvited(int fd) const
{
    return _invitedClients.find(fd) != _invitedClients.end();
}

bool Channel::isMember(int fd) const
{
    for (size_t i = 0; i < _clientsInChannel.size(); i++)
    {
        if (_clientsInChannel[i]->getFd() == fd)
            return true;
    }
    return false;
}

void Channel::setTopic(std::string topic) { _channelTopic = topic; }

void Channel::setPassword(std::string password)
{
    _channelPassword = password;
    _hasPassword = !password.empty();
}

void Channel::setInviteOnly(bool value) { _inviteOnly = value; }
void Channel::setTopicRestricted(bool value) { _topicRestricted = value; }

void Channel::setUserLimit(size_t limit)
{
    _userLimit = limit;
    _hasUserLimit = true;
}

void Channel::removeUserLimit()
{
    _userLimit = 0;
    _hasUserLimit = false;
}

void Channel::addClient(Client *client)
{
    _clientsInChannel.push_back(client);
}

void Channel::removeClient(int fd)
{
    for (size_t i = 0; i < _clientsInChannel.size(); i++)
    {
        if (_clientsInChannel[i]->getFd() == fd)
        {
            _clientsInChannel.erase(_clientsInChannel.begin() + i);
            break;
        }
    }
    _operators.erase(fd);
    _invitedClients.erase(fd);
}

void Channel::addOperator(int fd)
{
    _operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
    _operators.erase(fd);
}

void Channel::addInvited(int fd)
{
    _invitedClients.insert(fd);
}

void Channel::removeInvited(int fd)
{
    _invitedClients.erase(fd);
}

Client *Channel::getBot() const { return _botClient; }

void Channel::setBot(Client *bot) { _botClient = bot; }

void Channel::broadcastToChannel(std::string message, int excludeFd)
{
    for (size_t i = 0; i < _clientsInChannel.size(); i++)
    {
        if (_clientsInChannel[i]->getFd() != excludeFd) // excludefd c'est l'emetteur du message au cas ou
        {
            std::string fullMessage = message + "\r\n";
            send(_clientsInChannel[i]->getFd(), fullMessage.c_str(), fullMessage.length(), 0);
        }
    }
}

Channel *Server::CreateChannel(std::string name, Client *client)
{
    Channel *newChannel = new Channel(name);
    newChannel->addClient(client);
    newChannel->addOperator(client->getFd());
    this->ChannelMap[name] = newChannel;
    return newChannel;
}

Client *Server::GetClientByNickname(std::string nickname)
{
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        if (this->_ServerClients[i]->getNickname() == nickname)
            return this->_ServerClients[i];
    }
    return NULL;
}

Channel *Server::GetChannelByName(std::string name)
{
    std::map<std::string, Channel *>::iterator it = this->ChannelMap.find(name);
    if (it != this->ChannelMap.end())
        return it->second;
    return NULL;
}

void Server::RemoveChannelIfEmpty(std::string name)
{
    Channel *channel = GetChannelByName(name);

    if (channel && channel->getClients().empty())
    {
        delete channel;
        this->ChannelMap.erase(name);
        std::cout << "The " << name << " channel has been erased\n";
    }
}

void Server::RemoveClientFromAllChannels(int fd)
{
    std::map<std::string, Channel *>::iterator it;
    std::vector<std::string> channelsToCheck;

    for (it = this->ChannelMap.begin(); it != this->ChannelMap.end(); ++it)
    {
        if (it->second->isMember(fd))
        {
            Client *client = GetClientByFd(fd);
            if (client)
            {
                std::string partMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost QUIT :Client disconnected";
                it->second->broadcastToChannel(partMsg, fd);
            }
            it->second->removeClient(fd);
            channelsToCheck.push_back(it->first);
        }
    }
    for (size_t i = 0; i < channelsToCheck.size(); i++)
    {
        RemoveChannelIfEmpty(channelsToCheck[i]);
    }
}