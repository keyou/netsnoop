#pragma once

#include <netinet/in.h>

#include <string>
#include "netsnoop.h"

#define MAX_UDP_LENGTH 64*1024

class Sock
{
public:
    static int CreateSocket(int type, int protocol);
    static int Bind(int fd_, std::string ip, int port);
    static int Connect(int fd_,std::string ip, int port);
    static ssize_t Send(int fd_, const char *buf, size_t size);
    static ssize_t Recv(int fd_, char *buf, size_t size);
    static int GetLocalAddress(int fd_,std::string& ip,int& port);
    static int GetPeerAddress(int fd_,std::string& ip,int& port);
    static int SockAddrToStr(sockaddr_in* sockaddr,std::string &ip,int &port);
    static int StrToSockAddr(const std::string& ip,int port,sockaddr_in* sockaddr);

    int Initialize();
    int Bind(std::string ip, int port);
    int Connect(std::string ip,int port);
    virtual ssize_t Send(const char *buf, size_t size) const;
    virtual ssize_t Recv(char *buf, size_t size) const;
    int GetLocalAddress(std::string& ip,int& port);
    int GetPeerAddress(std::string& ip,int& port);

    virtual int Listen(int count) = 0;
    virtual int Accept() = 0;

    int GetFd(){return fd_;}

protected:
    Sock(int type, int protocol);
    Sock(int type, int protocol, int fd);
    virtual int InitializeEx(int fd) const { return fd; };

    static int GetSockAddr(const std::string& ip,in_addr* addr);

    int fd_;
    int type_;
    int protocol_;
    std::string local_ip_;
    int local_port_;
    std::string remote_ip_;
    int remote_port_;

    virtual ~Sock();

    DISALLOW_COPY_AND_ASSIGN(Sock);
};