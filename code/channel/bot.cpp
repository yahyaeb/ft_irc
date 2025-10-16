#include "../../resources/Irc.hpp"

void Server::botManager(int fd, std::string channelName, Channel *channel, Client *client)
{
    (void)fd;
    (void)client;

    Client *bot = new Client();
    bot->setFd(-1);
    bot->setNickname("ChannelBot");
    bot->setUsername("bot");
    bot->setRealname("Channel Information Bot");
    bot->setAuthenticated(true);
    bot->setRegistered(true);

    channel->setBot(bot);

    std::string welcomeMsg = ":ChannelBot!bot@localhost PRIVMSG " + channelName + " :Welcome to " + channelName + "! Type !help for available commands.";
    channel->broadcastToChannel(welcomeMsg, -1);

    std::cout << "Bot created for channel: " << channelName << std::endl;
}

void Server::handleBotCommand(Channel *channel, Client *client, std::string message)
{
    (void)client;

    if (message.find("!help") == 0)
    {
        std::string helpMsg = ":ChannelBot!bot@localhost PRIVMSG " + channel->getName() +
                              " :Available commands: !help, !rules, !info, !users";
        channel->broadcastToChannel(helpMsg, -1);
    }
    else if (message.find("!rules") == 0)
    {
        std::string rulesMsg = ":ChannelBot!bot@localhost PRIVMSG " + channel->getName() +
                               " :1. Be respectful 2. No spam 3. Stay on topic";
        channel->broadcastToChannel(rulesMsg, -1);
    }
    else if (message.find("!info") == 0)
    {
        std::string topic = channel->getTopic().empty() ? "No topic set" : channel->getTopic();
        std::string infoMsg = ":ChannelBot!bot@localhost PRIVMSG " + channel->getName() +
                              " :Channel: " + channel->getName() + " | Topic: " + topic;
        channel->broadcastToChannel(infoMsg, -1);
    }
    else if (message.find("!users") == 0)
    {
        std::ostringstream oss;
        oss << channel->getClients().size();
        std::string userCount = ":ChannelBot!bot@localhost PRIVMSG " + channel->getName() +
                                " :Current users: " + oss.str();
        channel->broadcastToChannel(userCount, -1);
    }
}