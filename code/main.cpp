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
    }
}
