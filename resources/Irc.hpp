#ifndef IRC_HPP
#define IRC_HPP
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>     
#include <netinet/in.h>     
#include <arpa/inet.h>      
#include <unistd.h>         
#include <netdb.h>          
#include <cstring>
#include <poll.h>
#include <cerrno>
#include <fcntl.h>
#include <sstream>
#include <map>
#include <unordered_set>

class Client
{
    private:
    int _Fd;
    std::string _ClientIp;
    std::string _Nickname;
    std::string _Username;
    std::string _Realname;
    bool        _IsAuthenticated;
    bool        _IsRegistered;
    std::string _Buffer;

    public:
    Client();
    int     getFd(void);
    void    setFd(int fd);
    void    setIp(std::string ip);

    std::string getNickname(void);
    std::string getUsername(void);
    bool        isAuthenticated(void);
    bool        isRegistered(void);
    std::string getBuffer(void);

    void    setNickname(std::string nickname);
    void    setUsername(std::string username);
    void    setRealname(std::string realname);
    void    setAuthenticated(bool auth);
    void    setRegistered(bool reg);
    void    appendBuffer(std::string data);
    void    clearBuffer(void);
};

class Server
{
    private:
    int _ServerPort;
    int _ServerSocket;
    std::string _ServerPassword;
    bool static _Signal;
    std::vector<Client> _ServerClients;
    std::vector<struct pollfd> _pollFds;


    public:
    Server();
    void    ServerInit(int port, std::string password);
    void    ServerSocketCreation();
    void    AcceptNewClient();
    void    ReceiveNewData(int fd);
    static void HandleSignal(int signum);
    void    closeFds();
    void    ClearClients(int fd);
    void    HandleClientOutput(char *buffer);

    void    HandleClientMessage(int fd, std::string message);
    void    HandlePassCommand(int fd, std::string args);
    void    HandleNickCommand(int fd, std::string args);
    void    HandleUserCommand(int fd, std::string args);
    void    HandleJoinCommand(int fd, std::string args);

    void    SendToClient(int fd, std::string message);
    Client* GetClientByFd(int fd);
    std::vector<std::string> SplitMessage(std::string message);
};

bool parsePortAndPswd(char *port, char *password);
#endif