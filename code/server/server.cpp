#include "../../resources/Irc.hpp"

Server::Server()
{
    this->_ServerSocket = -1;
}
bool Server::_Signal = false;

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
void Server::HandleSignal(int signum)
{
    (void)signum;
    Server::_Signal = true;
}

void Server::ServerInit(int port, std::string password)
{
    this->_ServerPort = port;
    this->_ServerPassword = password;
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

void Server::ServerSocketCreation(void)
{
    struct sockaddr_in ServerAdress;
    struct pollfd Polls;
    int flag;

    ServerAdress.sin_family = AF_INET;
    ServerAdress.sin_addr.s_addr = INADDR_ANY;
    ServerAdress.sin_port = htons(this->_ServerPort);
    this->_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->_ServerSocket == -1)
        throw(std::runtime_error("Error: failed to create server socket\n"));
    flag = 1;
    if (setsockopt(this->_ServerSocket, SOL_SOCKET, SO_REUSEADDR, &flag,
                   sizeof(flag)) == -1)
        throw(std::runtime_error("Error: adress already in use\n"));
    if (fcntl(this->_ServerSocket, F_SETFL, O_NONBLOCK) == -1)
        throw(std::runtime_error("Error: could not set socket in non blocking mode\n"));
    if (bind(this->_ServerSocket, (struct sockaddr *)&ServerAdress,
             sizeof(ServerAdress)) == -1)
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
    Client *client;
    struct sockaddr_in clientAdress;
    struct pollfd clientPollFd;
    socklen_t len;
    int clientSocket;

    client = new Client();
    len = sizeof(clientAdress);
    clientSocket = accept(this->_ServerSocket, (sockaddr *)&clientAdress, &len);
    if (clientSocket == -1)
        throw(std::runtime_error("Error: client socket creation failed\n"));
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1)
        throw(std::runtime_error("Error: failed to set client socket in non blocking mode\n"));
    clientPollFd.fd = clientSocket;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;
    client->setFd(clientSocket);
    client->setIp(inet_ntoa(clientAdress.sin_addr));
    this->_ServerClients.push_back(client);
    this->_pollFds.push_back(clientPollFd);
    std::cout << "Client <" << clientSocket << "> connected to the server!\n";
}

void Server::ReceiveNewData(int fd)
{
    char buffer[1024];
    ssize_t receivedBytes;
    Client *client;
    size_t lastNewline;

    memset(buffer, 0, sizeof(buffer));
    receivedBytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (receivedBytes <= 0)
    {
        std::cout << "Client <" << fd << "> disconnected see you next time!\n";
        this->ClearClients(fd);
        close(fd);
    }
    else
    {
        buffer[receivedBytes] = '\0';
        client = this->GetClientByFd(fd);
        if (!client)
            return;
        client->appendBuffer(std::string(buffer));
        std::vector<std::string> lines = this->SplitMessage(client->getBuffer());
        for (size_t i = 0; i < lines.size(); i++)
        {
            std::cout << "Client<" << fd << ">: " << lines[i] << std::endl;
            this->HandleClientMessage(fd, lines[i]);
        }
        lastNewline = client->getBuffer().find_last_of("\n");
        if (lastNewline != std::string::npos)
        {
            std::string remaining = client->getBuffer().substr(lastNewline + 1);
            client->clearBuffer();
            client->appendBuffer(remaining);
        }
    }
}

void Server::HandleClientMessage(int fd, std::string message)
{
    size_t spacePos;

    spacePos = message.find(' ');
    std::string command;
    std::string args;
    if (spacePos == std::string::npos)
    {
        command = message;
        args = "";
    }
    else
    {
        command = message.substr(0, spacePos);
        args = message.substr(spacePos + 1);
    }
    if (command == "PASS")
        HandlePassCommand(fd, args);
    else if (command == "NICK")
        HandleNickCommand(fd, args);
    else if (command == "USER")
        HandleUserCommand(fd, args);
    else if (command == "JOIN")
        HandleJoinCommand(fd, args);
    else if (command == "CAP")
        return;
    else if (command == "PING")
    {
        if (args.empty())
            SendToClient(fd, ":server PONG server");
        else
            SendToClient(fd, ":server PONG server :" + args);
    }
    else if (command == "PRIVMSG")
        HandlePrivmsgCommand(fd, args);
    else if (command == "KICK")
        HandleKickCommand(fd, args);
    else if (command == "INVITE")
        HandleInviteCommand(fd, args);
    else if (command == "TOPIC")
        HandleTopicCommand(fd, args, command);
    else if (command == "MODE")
        HandleModeCommand(fd, args);
    else
        SendToClient(fd, ":server 421 * " + command + " :Unknown command");
}

void Server::HandlePassCommand(int fd, std::string args)
{
    Client *client;

    client = GetClientByFd(fd);
    if (!client)
        return;
    if (client->isAuthenticated())
    {
        SendToClient(fd, ":server 462 * :You may not reregister");
        return;
    }
    if (args == this->_ServerPassword)
    {
        client->setAuthenticated(true);
        std::cout << "Client <" << fd << ">: "
                  << "authenticated successfully\n";
    }
    else
    {
        SendToClient(fd, ":server 464 * :Password incorrect");
        std::cout << "Client authentification failed\n";
        ClearClients(fd);
        close(fd);
        return;
    }
}

void Server::HandleNickCommand(int fd, std::string args)
{
    Client *client;

    client = this->GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isAuthenticated())
    {
        SendToClient(fd, ":server 451 * :You have not registered");
        ClearClients(fd);
        close(fd);
        return;
    }
    if (args.empty())
    {
        SendToClient(fd, ":server 431 * :No nickname given");
        return;
    }
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        if (this->_ServerClients[i]->getNickname() == args && this->_ServerClients[i]->getFd() != fd)
        {
            SendToClient(fd, ":server 433 * " + args + " :Nickname is already in use");
            return;
        }
    }
    client->setNickname(args);
    std::cout << "Client<" << fd << "> set nickname to: " << args << std::endl;
    if (!client->getUsername().empty() && !client->isRegistered())
    {
        client->setRegistered(true);
        SendToClient(fd, ":server 001 " + client->getNickname() + " :Welcome to the IRC Network!");
        std::cout << "Client <" << fd << "> is now registered!\n";
    }
}

void Server::HandleUserCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isAuthenticated())
    {
        SendToClient(fd, ":server 451 * :You have not registered");
        ClearClients(fd);
        close(fd);
        return;
    }
    std::istringstream iss(args);
    std::string username, mode, unusedParam, realname;

    iss >> username >> mode >> unusedParam;
    std::getline(iss, realname);
    if (!realname.empty() && realname[0] == ' ')
        realname = realname.substr(1);
    if (!realname.empty() && realname[0] == ':')
        realname = realname.substr(1);
    client->setUsername(username);
    client->setRealname(realname);

    if (!client->getNickname().empty() && !client->isRegistered())
    {
        client->setRegistered(true);
        SendToClient(fd, ":server 001 " + client->getNickname() + " :Welcome to the IRC Network!");
        std::cout << "Client <" << fd << "> is now registered!\n";
    }
}