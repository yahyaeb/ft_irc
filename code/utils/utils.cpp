#include "../../resources/Irc.hpp"

bool checkPortNb(int portNb)
{
    if (portNb >= 1024 && portNb <= 65535)
        return true;
    else
        return false;
    return true;
}

bool parsePortAndPswd(char *port, char *password)
{
    int portNb = atoi(port);
    if (strlen(port) == 0 || strlen(password) == 0 || strlen(password) >= 255)
        return false;
    for (int i = 0; port[i]; i++)
    {
        if (!isdigit(port[i]) || port[i] == '+' || port[i] == '-' || port[i] == ' ' || port[i] == '\t' || !checkPortNb(portNb))
            return false;
    }
    for (int i = 0; password[i]; i++)
    {
        if (!isprint(password[i]))
            return false;
    }
    return true;
}

void Server::closeFds()
{
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        close(this->_ServerClients[i]->getFd());
        delete this->_ServerClients[i];
    }
    std::map<std::string, Channel *>::iterator it;
    for (it = this->ChannelMap.begin(); it != this->ChannelMap.end(); it++)
        delete it->second;
    this->ChannelMap.clear();
    if (this->_ServerSocket != -1)
        close(this->_ServerSocket);
}

Client *Server::GetClientByFd(int fd)
{
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        if (this->_ServerClients[i]->getFd() == fd)
            return this->_ServerClients[i];
    }
    return NULL;
}

void Server::SendToClient(int fd, std::string message)
{
    if (message.find("\r\n") == std::string::npos)
        message += "\r\n";
    if (send(fd, message.c_str(), message.length(), 0) == -1)
        throw(std::runtime_error("Error: could not send message to client\n"));
}

std::vector<std::string> Server::SplitMessage(std::string message)
{
    std::vector<std::string> lines;
    std::string line;

    for (size_t i = 0; i < message.length(); i++)
    {
        if (message[i] == '\r' && i + 1 < message.length() && message[i + 1] == '\n')
        {
            if (!line.empty())
                lines.push_back(line);
            line.clear();
            i++; // saute le /n
        }
        else if (message[i] == '\n') // netcat fix
        {
            if (!line.empty())
                lines.push_back(line);
            line.clear();
        }
        else if (message[i] != '\r')
            line += message[i];
    }
    return lines;
}

std::vector<std::string> Server::SplitChannels(std::string channelName)
{
    std::vector<std::string> lines;
    std::string line;

    for (size_t i = 0; i < channelName.length(); i++)
    {
        if (channelName[i] == ',')
        {
            if (!line.empty())
                lines.push_back(line);
            line.clear();
        }
        else
            line += channelName[i];
    }
    if (!line.empty())
        lines.push_back(line);

    return lines;
}


void Server::ClearClients(int fd)
{
    RemoveClientFromAllChannels(fd);
    for (size_t i = 0; i < this->_pollFds.size(); i++)
    {
        if (this->_pollFds[i].fd == fd)
        {
            this->_pollFds.erase(this->_pollFds.begin() + i);
            break;
        }
    }
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        if (this->_ServerClients[i]->getFd() == fd)
        {
            delete this->_ServerClients[i];
            this->_ServerClients.erase(this->_ServerClients.begin() + i);
            break;
        }
    }
}