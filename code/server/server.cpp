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

void    Server::ServerInit(int port, std::string password)
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
        Client *client = this->GetClientByFd(fd);
        if (!client)
            return ;
        client->appendBuffer(std::string(buffer));
        std::vector<std::string> lines = this->SplitMessage(client->getBuffer());
        for (size_t i = 0; i < lines.size(); i++)
        {
            std::cout << "Client<" << fd << ">: " << lines[i] << std::endl;
            this->HandleClientMessage(fd, lines[i]);
        }
        std::string remaining = client->getBuffer();
        size_t lastNewline = remaining.rfind("\r\n");
        if (lastNewline != std::string::npos)
        {
            client->clearBuffer();
            if (lastNewline + 2 < remaining.length())
                client->appendBuffer(remaining.substr(lastNewline + 2));
        }
    }
}

void    Server::HandleClientMessage(int fd, std::string message)
{
    size_t spacePos = message.find(' ');
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
        return ;
    else if (command == "PING")
    {
        if (args.empty())
            SendToClient(fd, "PONG");
        else
            SendToClient(fd, "PONG " + args);
    }
    else
        SendToClient(fd, "421 * " + command + " :Unknown command");
}

void    Server::HandlePassCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);

    if (!client)
        return ;
    if (client->isAuthenticated())
    {
        SendToClient(fd, "462 :you may not register");
        return ;
    }
    if (args == this->_ServerPassword)
    {
        client->setAuthenticated(true);
        std::cout << "Client <" << fd << ">: " << "authenticated successfully\n";
    }
    else
    {
        SendToClient(fd, "464: Password incorrect");
        std::cout << "Client authentification failed\n";
        ClearClients(fd);
        close(fd);
        return ;
    }

}

void Server::HandleNickCommand(int fd, std::string args)
{
    Client *client = this->GetClientByFd(fd);
    if (!client)
        return;
    if (!client->isAuthenticated())
    {
        SendToClient(fd, "451 :You have not registered (send PASS first)");
        ClearClients(fd);
        close(fd);
        return ;
    }
    if (args.empty())
    {
        SendToClient(fd, "431 :No nickname given");
        return;
    }
    for (size_t i = 0; i < this->_ServerClients.size(); i++)
    {
        if (this->_ServerClients[i].getNickname() == args && this->_ServerClients[i].getFd() != fd)
        {
            srand(time(NULL));
            char suffix = 'A' + rand()%26;
            this->_ServerClients[i].setNickname(this->_ServerClients[i].getNickname() + suffix);
            SendToClient(fd, "433 :Nickname is already in use");
            return;
        }
    }
    client->setNickname(args);
    std::cout << "Client<" << fd << "> set nickname to: " << args << std::endl;
    if (!client->getUsername().empty() && !client->isRegistered())
    {
        client->setRegistered(true);
        SendToClient(fd, "001 " + client->getNickname() + " :Welcome to the IRC Network!");
        std::cout << "Client <" << fd << "> is now registered!\n";
    }
}

void   Server::HandleUserCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);
    if (!client)
        return ;
    if (!client->isAuthenticated())
    {
        SendToClient(fd, "451 :You have not registered (send PASS first)");
        ClearClients(fd);
        close(fd);
        return ;
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
        SendToClient(fd, "001 " + client->getNickname() + " :Welcome to the IRC Network!");
        std::cout << "Client <" << fd << "> is now registered!\n";
    }
}

void Server::HandleJoinCommand(int fd, std::string args)
{
    Client *client = GetClientByFd(fd);

    if (!client)
        return ;
    if (!client->isRegistered())
    {
        SendToClient(fd, "451 :You have not registered");
        return ;
    }
    std::istringstream iss(args);
    std::string channelName, channelPassword;
    iss >> channelName >> channelPassword;

    if (channelName.empty() || channelName[0] != '#')
    {
        SendToClient(fd, "403" + channelName + ":No such channel");
        return ;
    }

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
            SendToClient(fd, "443" + channelName + ":client already in channel");
            return;
        }
        if (channel->isInviteOnly() && !channel->isMember(fd))
        {
            SendToClient(fd, "473" + channelName + ":channel is in invite-only mode");
            return ;
        }
        if (channel->hasPassword() && channel->getPassword() != channelPassword)
        {
            SendToClient(fd, "475" + channelName + ":cannot join channel, wrong password");
            return ;
        }
        if (channel->hasUserLimit() && channel->getClients().size() >= channel->getUserLimit())
        {
            SendToClient(fd, "471" + channelName + ":channel's user limit reached");
            return ;
        }
    }
    channel->addClient(client);
    if (channel->isInvited(fd))
        channel->removeInvited(fd);
    std::string joinMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost JOIN " + channelName;
    channel->broadcastToChannel(joinMsg, -1);
    if (!channel->getTopic().empty())
    {
        SendToClient(fd, "332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
    }
    else
    {
        SendToClient(fd, "331 " + client->getNickname() + " " + channelName + " :No topic is set");
    }
    std::string userList = "353 " + client->getNickname() + " = " + channelName + " :";
    std::vector<Client*> clients = channel->getClients();
    for (size_t i = 0; i < clients.size(); i++)
    {
        if (channel->isOperator(clients[i]->getFd()))
            userList += "@";
        userList += clients[i]->getNickname();
        if (i < clients.size() - 1)
            userList += " ";
    }
    SendToClient(fd, userList);
    SendToClient(fd, "366 " + client->getNickname() + " " + channelName + " :End of /NAMES list");
    std::cout << "Client <" << fd << "> (" << client->getNickname() << ") joined " << channelName << std::endl;
}