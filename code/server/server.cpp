#include "../../resources/Irc.hpp"

Server::Server(){this->_SocketServFd = -1;}

void Server::ClearClients(int fd)
{
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
        if (this->_ServerClients[i].getFd() == fd)
        {
            this->_ServerClients.erase(this->_ServerClients.begin() + i);
            break;
        }
    }
}

void    Server::ServerInit(void)
{
    this->_ServerPort = 4444;
    this->ServerSocketCreation();

    std::cout << "Server <" << this->_SocketServFd << "> connected!\n";
    std::cout << "The server is waiting to accept a connection...\n";
}

void    Server::ServerSocketCreation(void)
{
    
}