#include "../../resources/Irc.hpp"

Client::Client(void){}
int     Client::getFd(void) {return this->_Fd;}
void    Client::setFd(int fd) {this->_Fd = fd;}
void    Client::setIp(std::string ip) {this->_ClientIp = ip;}
