
#include <string.h>

#include <string>
#include <cassert>

#include "tcp.h"
#include "netsnoop.h"
#include "sock.h"

Tcp::Tcp() : Sock(SOCK_STREAM, IPPROTO_TCP) {}
Tcp::Tcp(int fd) : Sock(SOCK_STREAM, IPPROTO_TCP, fd) {}
int Tcp::Listen(int count)
{
    ASSERT(fd_ > 0);

    if (listen(fd_, count) < 0)
    {
        LOGEP("listen error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }

    return 0;
}

int Tcp::Accept()
{
    ASSERT(fd_ > 0);

    int peerfd;
    int result;
    fd_set fds;
    FD_ZERO(&fds);

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr, 0, sizeof(peeraddr));

    if ((peerfd = accept(fd_, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        LOGEP("accept error: %s(errno: %d)", strerror(errno), errno);
        return -1;
    }
    LOGDP("accept tcp: %s:%d", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
    return peerfd;
}

int Tcp::InitializeEx(int fd) const
{
    int opt = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGEP("setsockopt TCP_NODELAY error: %s(errno: %d)", strerror(errno), errno);
        closesocket(fd);
        return -1;
    }
    opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGEP("setsockopt TCP_NODELAY error: %s(errno: %d)", strerror(errno), errno);
        closesocket(fd);
        return -1;
    }

#ifndef WIN32
    int keepcnt = 4;
    int keepidle = 5;
    int keepintvl = 5;

    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int))<0)
    {
        LOGEP("setsocketopt TCP_KEEPCNT error: %s(errno: %d)",strerror(errno),errno);
        closesocket(fd);
        return -1;
    }
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int))<0)
    {
        LOGEP("setsocketopt TCP_KEEPIDLE error: %s(errno: %d)",strerror(errno),errno);
        closesocket(fd);
        return -1;
    }
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int))<0)
    {
        LOGEP("setsocketopt TCP_KEEPINTVL error: %s(errno: %d)",strerror(errno),errno);
        closesocket(fd);
        return -1;
    }
#endif // !WIN32
    return fd;
}
