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

class Client
{
    private:
    int _Fd;
    std::string _ClientIp;

    public:
    Client();
    int     getFd(void);
    void    setFd(int fd);
    void    setIp(std::string ip);
};

class Server
{
    private:
    int _ServerPort;
    int _SocketServFd;
    std::vector<Client> _ServerClients;
    std::vector<struct pollfd> _pollFds;


    public:
    Server();
    void    ServerInit();
    void    ServerSocketCreation();
    void    AcceptNewClient();
    void    ReceiveNewData(int fd);
    static void SignalHandler(int signum);
    void    closeFds();
    void    ClearClients(int fd);
};

bool parsePortAndPswd(char *port, char *password);
#endif