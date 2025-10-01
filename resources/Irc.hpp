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
#include <cerrno> 
bool parsePortAndPswd(char *port, char *password);
void clientConnection(void);
#endif