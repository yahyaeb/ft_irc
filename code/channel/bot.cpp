#include "../../resources/Irc.hpp"

void    Server::botManager(int fd, Channel *channel, std::string channelName, Client *client)
{
    (void)fd;
    time_t t = time(NULL);
    struct tm date = *localtime(&t);
    std::ostringstream oss;
    std::ostringstream oss1;
    std::ostringstream oss2;

    int year = date.tm_year + 1900;
    int month = date.tm_mon + 1;
    int day = date.tm_mday;
    oss << year;
    oss1 << month;
    oss2 << day;
    std::string botMessage = ":" + client->getNickname() + "!" + client->getUsername() + "@localhost " + channelName + " channel, the current date is " + oss.str() + "-" + oss1.str() + "-" + oss2.str();
    channel->broadcastToChannel(botMessage, -1);
}