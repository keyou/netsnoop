#pragma once

#include <netinet/in.h>

#include <string>
#include "netsnoop.h"

class Sock
{
public:
    static int CreateSocket(int type, int protocol);
    static int Bind(int fd_, std::string ip, int port);
    static int Connect(int fd_,std::string ip, int port);
    static ssize_t Send(int fd_, const char *buf, size_t size);
    static ssize_t Recv(int fd_, char *buf, size_t size);

    int Initialize();
    int Bind(std::string ip, int port);
    int Connect(std::string ip,int port);
    ssize_t Send(const char *buf, size_t size) const;
    ssize_t Recv(char *buf, size_t size) const;

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

    ~Sock();

    DISALLOW_COPY_AND_ASSIGN(Sock);
};