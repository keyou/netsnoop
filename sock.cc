
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
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
        closesocket(sockfd);
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

    if (Logger::GetGlobalLogLevel() == LogLevel::LLVERBOSE)
    {
        char tmp[64] = {};
        strncpy(tmp, buf, sizeof(tmp) - 1);
        LOGVP("send(%ld): %s", result, tmp);
    }
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

    if (Logger::GetGlobalLogLevel() == LogLevel::LLVERBOSE)
    {
        char tmp[64] = {};
        strncpy(tmp, buf, sizeof(tmp) - 1);
        LOGVP("recv(%ld): %s", result, tmp);
    }
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
    return GetLocalAddress(fd_, ip, port);
}

int Sock::GetPeerAddress(std::string &ip, int &port)
{
    return GetPeerAddress(fd_, ip, port);
}

//static
int Sock::GetLocalAddress(int fd_, sockaddr_in *localaddr)
{
    socklen_t localaddr_length = sizeof(sockaddr_in);
    memset(localaddr, 0, localaddr_length);
    if (getsockname(fd_, (sockaddr *)localaddr, &localaddr_length) < 0)
    {
        LOGEP("getsockname error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    return 0;
}
//static
int Sock::GetPeerAddress(int fd_, sockaddr_in *peeraddr)
{
    socklen_t peeraddr_length = sizeof(sockaddr_in);
    memset(peeraddr, 0, peeraddr_length);
    if (getpeername(fd_, (sockaddr *)peeraddr, &peeraddr_length) < 0)
    {
        LOGEP("getpeername error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    return 0;
}

//static
int Sock::GetLocalAddress(int fd_, std::string &ip, int &port)
{
    sockaddr_in localaddr;
    int result = Sock::GetLocalAddress(fd_, &localaddr);
    ASSERT_RETURN(result >= 0, -1);
    ip = inet_ntoa(localaddr.sin_addr);
    port = ntohs(localaddr.sin_port);
    return 0;
}

//static
int Sock::GetPeerAddress(int fd_, std::string &ip, int &port)
{
    sockaddr_in peeraddr;
    int result = Sock::GetPeerAddress(fd_, &peeraddr);
    ASSERT_RETURN(result >= 0, -1);
    ip = inet_ntoa(peeraddr.sin_addr);
    port = ntohs(peeraddr.sin_port);
    return 0;
}

//static
int Sock::SockAddrToStr(sockaddr_in *sockaddr, std::string &ip, int &port)
{
    ip = inet_ntoa(sockaddr->sin_addr);
    port = ntohs(sockaddr->sin_port);
    return 0;
}

//static
int Sock::StrToSockAddr(const std::string &ip, int port, sockaddr_in *sockaddr)
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
int Sock::GetSockAddr(const std::string &ip, in_addr &addr)
{
    addr.s_addr = inet_addr(ip.c_str());
    if (addr.s_addr != -1)
        return 0;
    hostent *record = gethostbyname(ip.c_str());
    if (record == NULL)
    {
        LOGEP("gethostbyname error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    // TODO: in multithread(100),it crash here sometimes.
    addr = *(in_addr *)record->h_addr;
    return 0;
}

std::vector<std::string> Sock::Host2Ips(const std::string &host)
{
    std::vector<std::string> ips;
    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void *ptr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host.c_str(), NULL, &hints, &res);
    if (errcode != 0)
    {
        LOGEP("getaddrinfo error: %s (errno: %d)", strerror(errno), errno);
        return ips;
    }

    while (res)
    {
        inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);
        ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        inet_ntop(res->ai_family, ptr, addrstr, 100);
        printf("IPv%d address: %s (%s)\n", 4, addrstr, res->ai_canonname);
        res = res->ai_next;
    }

    return ips;
}

//static
std::vector<std::string> Sock::GetLocalIps()
{
    std::vector<std::string> ips;
#ifndef WIN32
    std::string ip;
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    
    int result = getifaddrs(&interfaces);
    if (result == 0) {
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                ip=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                if(ip.length()>0)
                {
                    ips.push_back(ip);
                    LOGVP("find local ip: %s",ip.c_str());
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    freeifaddrs(interfaces);
#else
    ips.push_back("0.0.0.0");
#endif // WIN32
    return ips;
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
        closesocket(fd_);
        fd_ = -1;
    }
}
