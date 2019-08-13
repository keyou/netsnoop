#pragma once

#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>

#include <ctime>
#include <ratio>
#include <chrono>

#include <memory>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <functional>
#include <thread>
#include <signal.h>

#include "sock.h"

//static
int Sock::CreateSocket(int type, int protocol)
{
    int sockfd;
    LOGV("create socket.\n");
    if ((sockfd = socket(AF_INET, type, protocol)) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGE("setsockopt SO_REUSEADDR error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
}
//static
int Sock::Bind(int fd_, std::string ip, int port)
{
    ASSERT(fd_ > 0);
    struct sockaddr_in localaddr;
    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &localaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton local error for %s\n", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }

    LOGV("bind %s:%d\n", ip.c_str(), port);
    if (bind(fd_, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    return 0;
}

//static
ssize_t Sock::Send(int fd_, const char *buf, size_t size)
{
    ASSERT(fd_ > 0);
    ssize_t result;
    if ((result = send(fd_, buf, size, 0)) < 0 || result != size)
    {
        LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    LOGV("send count: %ld\n", result);
    return result;
}

//static
ssize_t Sock::Recv(int fd_, char *buf, size_t size)
{
    ASSERT(fd_ > 0);
    ssize_t result = 0;
    if ((result = recv(fd_, buf, size, 0)) == -1)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            LOGE("recv error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        LOGE("recv timeout.\n");
        return ERR_TIMEOUT;
    }
    LOGV("recv count: %ld\n", result);
    return result;
}
int Sock::Initialize()
{
    int sockfd;
    int opt;

    if (fd_ > 0)
        return fd_;

    sockfd = CreateSocket(type_, protocol_);
    if (sockfd < 0)
        return -1;

    if (InitializeEx(sockfd) < 0)
        return -1;
    fd_ = sockfd;
    return sockfd;
}
int Sock::Bind(std::string ip, int port)
{
    local_ip_ = ip;
    local_port_ = port;
    return Bind(fd_, ip, port);
}
ssize_t Sock::Send(const char *buf, size_t size) const
{
    return Send(fd_, buf, size);
}

ssize_t Sock::Recv(char *buf, size_t size) const
{
    return Recv(fd_, buf, size);
}

int Sock::GetLocalAddress(std::string &ip, int &port)
{
    sockaddr_in localaddr;
    socklen_t localaddr_length = sizeof(localaddr);
    if (getsockname(fd_, (sockaddr *)&localaddr, &localaddr_length) < 0)
    {
        LOGE("getsockname error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    ip = inet_ntoa(localaddr.sin_addr);
    port = ntohs(localaddr.sin_port);
    return 0;
}

int Sock::GetPeerAddress(std::string& ip,int& port)
{
    sockaddr_in peeraddr;
    socklen_t peeraddr_length = sizeof(peeraddr);
    if (getpeername(fd_, (sockaddr *)&peeraddr, &peeraddr_length) < 0)
    {
        LOGE("getpeername error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    ip = inet_ntoa(peeraddr.sin_addr);
    port = ntohs(peeraddr.sin_port);
    return 0;
}

Sock::Sock(int type, int protocol)
    : fd_(0), type_(type), protocol_(protocol) {}
Sock::Sock(int type, int protocol, int fd)
    : fd_(fd), type_(type), protocol_(protocol) {}

Sock::~Sock()
{
    if (fd_ > 0)
    {
        LOGV("close socket: fd = %d\n", fd_);
        close(fd_);
        fd_ = -1;
    }
}
