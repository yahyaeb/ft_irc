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
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // ca c'est pour creer le socket du server
        //maintenat on doit definir l'adresse du server
        sockaddr_in serverAdress; //sockaddr_in c'est la structure de donnes qui stock l'adresse du socket
        serverAdress.sin_family = AF_INET; // le port il est en ipv4
        serverAdress.sin_port = htons(8080); // convertit le port en format rsx pour que toutes les machines peuvent communiquer;
        serverAdress.sin_addr.s_addr = INADDR_ANY; //accept les connections de toutes les IP
        bind(serverSocket, (struct sockaddr *)&serverAdress, sizeof(serverAdress)); //on va bind le socket du server a son adresse;
        listen(serverSocket, 5);

        int clientSocket = accept(serverSocket, NULL, NULL);

        char buffer[1024] = {0};
        recv(clientSocket, buffer, sizeof(buffer), 0);
        std::cout << "Message from client: " << buffer << std::endl;

        close(serverSocket);

        clientConnection();
    }
}

void clientConnection(void)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in clientAdress;

    clientAdress.sin_family = AF_INET;
    clientAdress.sin_port = htons(8080);
    clientAdress.sin_addr.s_addr = INADDR_ANY;
    connect(clientSocket, (struct sockaddr*)&clientAdress, sizeof(clientAdress));

    const char *message = "OE LE SERVEEEER OE\n";
    send(clientSocket, message, strlen(message), 0);

    close(clientSocket);

}
