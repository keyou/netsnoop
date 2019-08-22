#pragma once

#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

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


int join_mcast(int fd, ulong groupaddr);
int join_mcast(int fd, ulong groupaddr)
{
    struct ip_mreq mreq;

    if (IN_MULTICAST(ntohl(groupaddr)) == 0)
        return -1;

    mreq.imr_multiaddr.s_addr = groupaddr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
    {
        LOGEP("IP_ADD_MEMBERSHIP error: %s(errno: %d)",strerror(errno),errno);
        return -1;
    }

    LOGVP("multicast group joined");
    return 0;
}

//static
int Sock::CreateSocket(int type, int protocol)
{
    int sockfd;
    LOGVP("create socket.");
    if ((sockfd = socket(AF_INET, type, protocol)) < 0)
    {
        LOGEP("create socket error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGEP("setsockopt SO_REUSEADDR error: %s(errno: %d)", strerror(errno), errno);
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

    //if(GetSockAddr(ip,&localaddr.sin_addr)<0) return -1;

    if (inet_pton(AF_INET, ip.c_str(), &localaddr.sin_addr) <= 0)
    {
        LOGEP("inet_pton local error for %s", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }

    LOGVP("bind %s:%d", ip.c_str(), port);
    if (bind(fd_, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        LOGEP("bind error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }

    if (IN_MULTICAST(ntohl(localaddr.sin_addr.s_addr)))
        return join_mcast(fd_,localaddr.sin_addr.s_addr);

    return 0;
}

//static
int Sock::Connect(int fd_, std::string ip, int port)
{
    ASSERT(fd_ > 0);
    struct sockaddr_in remoteaddr;
    memset(&remoteaddr, 0, sizeof(remoteaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(port);

    //if(GetSockAddr(ip,&remoteaddr.sin_addr)<0) return -1;

    if (inet_pton(AF_INET, ip.c_str(), &remoteaddr.sin_addr) <= 0)
    {
        LOGEP("inet_pton remote error for %s", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }

    LOGVP("connect %s:%d", ip.c_str(), port);
    if (connect(fd_, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
    {
        LOGEP("connect error: %s(errno: %d)", strerror(errno), errno);
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
        LOGEP("send error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }

#ifdef _DEBUG
    char tmp[64] = {};
    strncpy(tmp, buf, sizeof(tmp)-1);
    LOGVP("send(%ld): %s", result, tmp);
#endif // _DEBUG
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
            LOGEP("recv error: %s(errno: %d)", strerror(errno), errno);
            return -1;
        }
        LOGEP("recv timeout.");
        return ERR_TIMEOUT;
    }
#ifdef _DEBUG
    char tmp[64] = {};
    strncpy(tmp, buf, sizeof(tmp)-1);
    LOGVP("recv(%ld): %s", result, tmp);
#endif // _DEBUG
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
int Sock::Connect(std::string ip, int port)
{
    remote_ip_ = ip;
    remote_port_ = port;
    return Connect(fd_, ip, port);
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
    return GetLocalAddress(fd_,ip,port);
}

int Sock::GetPeerAddress(std::string &ip, int &port)
{
    return GetPeerAddress(fd_,ip,port);
}

//static
int Sock::GetLocalAddress(int fd_, std::string &ip, int &port)
{
    sockaddr_in localaddr;
    socklen_t localaddr_length = sizeof(localaddr);
    if (getsockname(fd_, (sockaddr *)&localaddr, &localaddr_length) < 0)
    {
        LOGEP("getsockname error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    ip = inet_ntoa(localaddr.sin_addr);
    port = ntohs(localaddr.sin_port);
    return 0;
}

//static
int Sock::GetPeerAddress(int fd_, std::string &ip, int &port)
{
    sockaddr_in peeraddr;
    socklen_t peeraddr_length = sizeof(peeraddr);
    if (getpeername(fd_, (sockaddr *)&peeraddr, &peeraddr_length) < 0)
    {
        LOGEP("getpeername error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    ip = inet_ntoa(peeraddr.sin_addr);
    port = ntohs(peeraddr.sin_port);
    return 0;
}

//static
int Sock::SockAddrToStr(sockaddr_in* sockaddr,std::string &ip,int &port)
{
    ip = inet_ntoa(sockaddr->sin_addr);
    port = ntohs(sockaddr->sin_port);
    return 0;
}

//static
int Sock::StrToSockAddr(const std::string& ip,int port,sockaddr_in* sockaddr)
{
    //sockaddr_in peeraddr;
    //socklen_t peeraddr_size = sizeof(sockaddr_in);
    memset(sockaddr, 0, sizeof(sockaddr_in));
    sockaddr->sin_family = AF_INET;
    sockaddr->sin_port = htons(port);

    //if(GetSockAddr(ip,&sockaddr.sin_addr)<0) return -1;

    if (inet_pton(AF_INET, ip.c_str(), &sockaddr->sin_addr) <= 0)
    {
        LOGEP("inet_pton remote error for %s", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }
    return 0;
}

//static
int Sock::GetSockAddr(const std::string &ip, in_addr *addr)
{
    hostent *record = gethostbyname(ip.c_str());
    if (record == NULL)
    {
        LOGEP("gethostbyname error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    // TODO: in multi(100) thread,it crash here sometimes.
    *addr = *(in_addr *)record->h_addr;
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
        LOGVP("close socket: fd = %d", fd_);
        close(fd_);
        fd_ = -1;
    }
}
