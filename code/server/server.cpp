#include "../../resources/Irc.hpp"

Server::Server(){this->_ServerSocket = -1;}
bool Server::_Signal = false;
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
void    Server::HandleSignal(int signum)
{
    (void)signum;
    Server::_Signal = true;
}

void    Server::ServerInit(int port)
{
    this->_ServerPort = port;
    this->ServerSocketCreation();

    std::cout << "Server <" << this->_ServerSocket << "> connected!\n";
    std::cout << "The server is waiting to accept a connection...\n";
    while (Server::_Signal == false)
    {
        if (poll(&this->_pollFds[0], this->_pollFds.size(), -1) == -1 && Server::_Signal == false)
            throw(std::runtime_error("Error: call to poll failed"));
        for (size_t i = 0; i < this->_pollFds.size(); i++)
        {
            if (this->_pollFds[i].revents & POLLIN)
            {
                if (this->_pollFds[i].fd == this->_ServerSocket)
                    this->AcceptNewClient();
                else
                    this->ReceiveNewData(this->_pollFds[i].fd);
            }
        }
    }
    closeFds();
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

void Server::AcceptNewClient()
{
    Client client;
    struct sockaddr_in clientAdress;
    struct pollfd   clientPollFd;
    socklen_t       len = sizeof(clientAdress);

    int clientSocket = accept(this->_ServerSocket, (sockaddr *)&clientAdress, &len);
    if (clientSocket == -1)
        throw(std::runtime_error("Error: client socket creation failed\n"));
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1)
        throw(std::runtime_error("Error: failed to set client socket in non blocking mode\n"));
    clientPollFd.fd = clientSocket;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;

    client.setFd(clientSocket);
    client.setIp(inet_ntoa(clientAdress.sin_addr));

    this->_ServerClients.push_back(client);
    this->_pollFds.push_back(clientPollFd);

    std::cout << "Client <" << clientSocket << "> connected to the server!\n";
}

void Server::ReceiveNewData(int fd)
{
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));

    ssize_t receivedBytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (receivedBytes <= 0)
    {
        std::cout << "Client <" << fd << "> disconnected see you next time!\n";
        this->ClearClients(fd);
        close(fd);
    }
    else
    {
        buffer[receivedBytes] = '\0';
        std::cout << "Data from Client <" << fd << ">: " << buffer << std::endl;
        //ici je mettrai le code de parsing des donnes recues
        // HandleInput(buffer);
    }
}

// void    Server::HandleInput(char *buffer)
// {
//     std::string userInput(buffer);

//     if (userInput == "iheb\r\n")
//         std::cout << "hello iheb how are you today\n";
// }