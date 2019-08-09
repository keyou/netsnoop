
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

#include "tcp.h"
#include "netsnoop.h"

Tcp::Tcp() : Sock(SOCK_STREAM, IPPROTO_TCP) {}
Tcp::Tcp(int fd) : Sock(SOCK_STREAM, IPPROTO_TCP, fd) {}
int Tcp::Listen(int count)
{
    ASSERT(fd_ > 0);

    if (listen(fd_, count) < 0)
    {
        LOGE("listen error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    return 0;
}
int Tcp::Connect(std::string ip, int port)
{
    ASSERT(fd_ > 0);
    struct sockaddr_in remoteaddr;
    memset(&remoteaddr, 0, sizeof(remoteaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &remoteaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton remote error for %s\n", ip.c_str());
        return ERR_ILLEGAL_PARAM;
    }

    LOGV("connect %s %s:%d\n", protocol_ == IPPROTO_TCP ? "tcp" : "udp", ip.c_str(), port);
    if (connect(fd_, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    remote_ip_ = ip;
    remote_port_ = port;
    return 0;
}
int Tcp::Accept()
{
    ASSERT(fd_ > 0);

    int peerfd;
    int result;
    char remote_ip[20] = {0};
    fd_set fds;
    FD_ZERO(&fds);

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr, 0, sizeof(peeraddr));

    if ((peerfd = accept(fd_, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        LOGE("accept error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
#ifdef _DEBUG
    inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
    LOGV("accept tcp: %s:%d\n", remote_ip, ntohs(peeraddr.sin_port));
#endif
    return peerfd;
}

int Tcp::InitializeEx(int fd) const
{
    int opt = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGE("setsockopt TCP_NODELAY error: %s(errno: %d)\n", strerror(errno), errno);
        close(fd);
        return -1;
    }
    return fd;
}
