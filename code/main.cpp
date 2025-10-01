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
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            serv.ServerInit();
        }
        catch(const std::exception& e)
        {
            serv.closeFds();
            std::cerr << e.what() << '\n';
        }
        
    }
}
