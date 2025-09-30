#include "../resources/Irc.hpp"


int main (int argc, char **argv)
{
    if (argc != 3 || !parsePortAndPswd(argv[1], argv[2]))
    {
        std::cout << "womp womp\n";
        exit (1);
    }
    else
    {
        std::cout << "parse ok\n";
    }
}

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