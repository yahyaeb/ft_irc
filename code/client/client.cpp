#include "../../resources/Irc.hpp"

Client::Client(void){
    this->_Fd = -1;                     
    this->_IsAuthenticated = false;     
    this->_IsRegistered = false;        
}

int Client::getFd(void){return this->_Fd;}

void Client::setFd(int fd) {this->_Fd = fd;}

void Client::setIp(std::string ip) {this->_ClientIp = ip;}

std::string Client::getNickname(void) {return this->_Nickname;}

std::string Client::getUsername(void) {return this->_Username;}

bool Client::isAuthenticated(void) {return this->_IsAuthenticated;}

bool Client::isRegistered(void) {return this->_IsRegistered;}

std::string Client::getBuffer(void) {return this->_Buffer;}

void Client::setNickname(std::string nickname) {this->_Nickname = nickname;}

void Client::setUsername(std::string username) {this->_Username = username;}

void Client::setRealname(std::string realname) {this->_Realname = realname;}

void Client::setAuthenticated(bool auth) {this->_IsAuthenticated = auth;}

void Client::setRegistered(bool reg) {this->_IsRegistered = reg;}

void Client::appendBuffer(std::string data) {this->_Buffer += data;}

void Client::clearBuffer(void) {this->_Buffer.clear();}