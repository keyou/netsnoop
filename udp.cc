

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <cassert>

#include "netsnoop.h"
#include "udp.h"

Udp::Udp() : Sock(SOCK_DGRAM, IPPROTO_UDP) {}
Udp::Udp(int fd) : Sock(SOCK_DGRAM, IPPROTO_UDP, fd) {}
int Udp::Listen(int count)
{
    count_ = count;
    return 0;
}

int Udp::Connect(std::string ip, int port)
{
    ASSERT(fd_ > 0);
    struct sockaddr_in remoteaddr;
    socklen_t size = sizeof(remoteaddr);
    memset(&remoteaddr, 0, sizeof(remoteaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &remoteaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton remote error for %s\n", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }

    if (Connect(fd_, &remoteaddr, size) < 0)
        return -1;

    // char buf;
    // int try_count = 0;
    // while(true)
    // {
    //     if(Send(fd_,"",0) < 0 || Recv(fd_,&buf,1) <0)
    //     {
    //         if( (++try_count) < MAX_TRY_NUM) continue;
    //         LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
    //         return -1;
    //     }
    //     break;
    // }

    LOGV("Connect udp: %s:%d\n", ip.c_str(), port);

    remote_ip_ = ip;
    remote_port_ = port;
    return 0;
}

int Udp::Accept()
{
    ASSERT(fd_ > 0);
    int result;
    char buf[64] = {0};
    char remote_ip[20] = {0};

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr, 0, sizeof(peeraddr));

    memset(remote_ip, 0, sizeof(remote_ip));

    if ((result = recvfrom(fd_, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        LOGE("accept error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
    LOGV("accept udp from [%s:%d]: %s\n", remote_ip, ntohs(peeraddr.sin_port), buf);

    int peerfd = CreateSocket(type_, protocol_);
    if (Bind(peerfd, local_ip_, local_port_) < 0)
        return -1;
    if (Connect(peerfd, &peeraddr, peeraddr_size) < 0)
        return -1;
    if ((result = Send(peerfd, buf, result)) < 0)
        return -1;

    return peerfd;
}

//static
int Udp::Connect(int fd, sockaddr_in *remoteaddr, int size)
{
    if (connect(fd, (struct sockaddr *)remoteaddr, size) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    return 0;
}
