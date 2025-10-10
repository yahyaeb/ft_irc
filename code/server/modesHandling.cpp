#include "../../resources/Irc.hpp"

void Server::iMode(Channel *channel, bool adding, std::string appliedModes)
{
    channel->setInviteOnly(adding);
    if (adding)
        appliedModes += "+";
    else
        appliedModes += "-";
    appliedModes += "i";
}

void    Server::tMode(Channel *channel, bool adding, std::string appliedModes)
{
    channel->setTopicRestricted(adding);
    if (adding)
        appliedModes += "+";
    else
        appliedModes += "-";
    appliedModes += "t";   
}

void    Server::kMode(int fd, Channel *channel, bool adding, std::string appliedModes, std::string modeParam, std::string modeParams)
{
    if (adding)
    {
        if (modeParam.empty())
        {
            SendToClient(fd, "461 MODE :Not enough parameters");
            return;
        }
        channel->setPassword(modeParam);
        appliedModes += "+k";
        modeParams += " " + modeParam;
    }
    else
    {
        channel->setPassword("");
        appliedModes += "-k";
    }
}

void    Server::oMode(int fd, Channel *channel, bool adding, std::string appliedModes, std::string modeParam, std::string modeParams, std::string channelName)
{
    if (modeParam.empty())
    {
        SendToClient(fd, "461 MODE :Not enough parameters");
        return;
    }

    Client *targetClient = GetClientByNickname(modeParam);
    if (!targetClient)
    {
        SendToClient(fd, "401 " + modeParam + " :No such nick/channel");
        return;
    }

    if (!channel->isMember(targetClient->getFd()))
    {
        SendToClient(fd, "441 " + modeParam + " " + channelName + " :They aren't on that channel");
        return;
    }

    if (adding)
    {
        channel->addOperator(targetClient->getFd());
        appliedModes += "+";
    }
    else
    {
        channel->removeOperator(targetClient->getFd());
        appliedModes += "-";
    }
    appliedModes += "o";
    modeParams += " " + modeParam;
}

void    Server::lMode(int fd, Channel *channel, bool adding, std::string appliedModes, std::string modeParam, std::string modeParams, std::string channelName)
{
    if (adding)
    {
        if (modeParam.empty())
        {
            SendToClient(fd, "461 MODE :Not enough parameters");
            return;
        }

        int limit = atoi(modeParam.c_str());
        if (limit <= 0)
        {
            SendToClient(fd, "696 " + channelName + " l :Invalid limit");
            return;
        }

        channel->setUserLimit(limit);
        appliedModes += "+l";
        modeParams += " " + modeParam;
    }
    else
    {
        channel->removeUserLimit();
        appliedModes += "-l";
    }
}