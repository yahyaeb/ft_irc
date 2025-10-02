#include "../resources/Irc.hpp"


int main (int argc, char **argv)
{
    Server serv;
    if (argc != 3 || !parsePortAndPswd(argv[1], argv[2]))
    {
        std::cerr << "Arguments error\n";
        exit (1);
    }
    else
    {
        try
        {
            signal(SIGINT, serv.HandleSignal);
            signal(SIGQUIT, serv.HandleSignal);
            serv.ServerInit(atoi(argv[1]));
        }
        catch(const std::exception& e)
        {
            serv.closeFds();
            std::cerr << e.what() << '\n';
        }
        
    }
}
