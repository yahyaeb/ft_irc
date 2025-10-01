#include "../../resources/Irc.hpp"

bool checkPortNb(int portNb)
{
    if (portNb >= 1 && portNb <= 65535)
        return true;
    else
        return false;
    return true;
}

bool parsePortAndPswd(char *port, char *password)
{
    int portNb = atoi(port);
    if (strlen(port) == 0 || strlen(password) == 0 || strlen(password) >= 255)
        return false;
    for (int i = 0; port[i]; i++)
    {
        if (!isdigit(port[i]) || port[i] == '+' 
            || port[i] == '-' || port[i] == ' '
            || port[i] == '\t' || !checkPortNb(portNb))
            return false;
    }
    for (int i = 0; password[i]; i++)
    {
        if (!isprint(password[i]))
            return false;
    }
    return true;
}