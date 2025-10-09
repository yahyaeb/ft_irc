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
#include <time.h>
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

class Channel
{
    private:
    std::string _channelName;
    std::string _channelTopic;
    std::string _channelPassword;
    std::vector<Client *> _clientsInChannel;
    std::unordered_set<int> _operators;
    std::unordered_set<int> _invitedClients;

    bool _inviteOnly;
    bool _topicRestricted;
    bool _hasPassword;
    bool _hasUserLimit;
    size_t _userLimit;

    public:
    Channel(std::string name);
    ~Channel();

    std::string getName() const;
    std::string getTopic() const;
    std::string getPassword() const;
    std::vector<Client *> getClients() const;
    size_t getUserLimit() const;

    bool isOperator(int fd) const;
    bool isInvited(int fd) const;
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    bool hasPassword() const;
    bool hasUserLimit() const;
    bool isMember(int fd) const;

    void setTopic(std::string topic);
    void setPassword(std::string password);
    void setInviteOnly(bool value);
    void setTopicRestricted(bool value);
    void setUserLimit(size_t limit);
    void removeUserLimit();
    void broadcastToChannel(std::string message, int excludeFd = -1);
    void addClient(Client* client);
    void removeClient(int fd);
    void addOperator(int fd);
    void removeOperator(int fd);
    void addInvited(int fd);
    void removeInvited(int fd);
};
class Server
{
    private:
    int _ServerPort;
    int _ServerSocket;
    std::string _ServerPassword;
    bool static _Signal;
    std::vector<Client*> _ServerClients;
    std::vector<struct pollfd> _pollFds;
    std::map<std::string, Channel *> ChannelMap; //nom du channel = channel;

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
    void    HandlePrivmsgCommand(int fd, std::string args);
    void    HandleKickCommand(int fd, std::string args);
    void    HandleInviteCommand(int fd, std::string args);
    void    HandleTopicCommand(int fd, std::string args);
    void    HandleModeCommand(int fd, std::string args);

    void    SendToClient(int fd, std::string message);
    Client* GetClientByFd(int fd);
    std::vector<std::string> SplitMessage(std::string message);
    Client* GetClientByNickname(std::string nickname);
    Channel* GetChannelByName(std::string name);
    Channel* CreateChannel(std::string name, Client* creator);
    void    RemoveChannelIfEmpty(std::string name);
    void    RemoveClientFromAllChannels(int fd);
};


bool parsePortAndPswd(char *port, char *password);
#endif