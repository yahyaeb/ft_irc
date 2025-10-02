#include "../../resources/Irc.hpp"

Server::Server(){this->_ServerSocket = -1;}

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

void    Server::ServerInit(int port)
{
    this->_ServerPort = port;
    this->ServerSocketCreation();

    std::cout << "Server <" << this->_ServerSocket << "> connected!\n";
    std::cout << "The server is waiting to accept a connection...\n";
}

void    Server::ServerSocketCreation(void)
{
    struct sockaddr_in ServerAdress;
    struct pollfd Polls;

    ServerAdress.sin_family = AF_INET;
    ServerAdress.sin_addr.s_addr = INADDR_ANY;
    ServerAdress.sin_port = htons(this->_ServerPort);

    this->_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->_ServerSocket == -1)
        throw(std::runtime_error("Error: failed to create server socket\n"));
    int flag = 1;
    if (setsockopt(this->_ServerSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1)
        throw(std::runtime_error("Error: adress already in use\n"));
    if (fcntl(this->_ServerSocket, F_SETFL, O_NONBLOCK) == -1)
        throw(std::runtime_error("Error: could not set socket in non blocking mode\n"));
    if (bind(this->_ServerSocket, (struct sockaddr *)&ServerAdress, sizeof(ServerAdress)) == -1)
        throw(std::runtime_error("Error: could not bind the server's socket to an IP adress\n"));
    if (listen(this->_ServerSocket, SOMAXCONN) == -1)
        throw(std::runtime_error("Error: listen() failed\n"));
    Polls.fd = this->_ServerSocket;
    Polls.events = POLLIN;
    Polls.revents = 0;
    this->_pollFds.push_back(Polls);
}