
#include <string>
#include <cassert>

#include "netsnoop.h"
#include "udp.h"
#include "sock.h"

Udp::Udp() : Sock(SOCK_DGRAM, IPPROTO_UDP) {}
Udp::Udp(int fd) : Sock(SOCK_DGRAM, IPPROTO_UDP, fd) {}
int Udp::Listen(int count)
{
    count_ = count;
    return 0;
}

int Udp::Accept()
{
    ASSERT(fd_ > 0);
    int result;
    char buf[64] = {0};
    char remote_ip[40] = {0};

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr, 0, sizeof(peeraddr));

    memset(remote_ip, 0, sizeof(remote_ip));

    if ((result = recvfrom(fd_, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        PSOCKETERROR("accept error");
        return -1;
    }

    inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
    LOGDP("accept udp from [%s:%d]: %s", remote_ip, ntohs(peeraddr.sin_port), buf);

    int peerfd = CreateSocket(type_, protocol_);
    if (Bind(peerfd, local_ip_, local_port_) < 0)
        return -1;
    if (connect(peerfd, (struct sockaddr *)&peeraddr, peeraddr_size) < 0)
    {
        PSOCKETERROR("connect error");
        return -1;
    }
    if ((result = Send(peerfd, buf, result)) < 0)
        return -1;

    return peerfd;
}

//static
ssize_t Udp::SendTo(const std::string &buf, sockaddr_in *peeraddr)
{
    int result;

    if ((result = sendto(fd_, buf.c_str(), buf.length(), 0, (struct sockaddr *)peeraddr, sizeof(sockaddr_in))) < 0)
    {
        PSOCKETERROR("sendto error");
        return -1;
    }
    if (Logger::GetGlobalLogLevel() == LogLevel::LLVERBOSE)
    {
        char *ip = inet_ntoa(peeraddr->sin_addr);
        int port = ntohs(peeraddr->sin_port);
        LOGDP("sendto(%s:%d)(%d): %s", ip, port, result, buf.substr(0, 64).c_str());
    }
    return result;
}

//static
ssize_t Udp::RecvFrom(std::string &buf, sockaddr_in *peeraddr)
{
    int result;
    char remote_ip[64] = {0};

    socklen_t peeraddr_size = sizeof(sockaddr_in);
    memset(peeraddr, 0, sizeof(sockaddr_in));

    if ((result = recvfrom(fd_, &buf[0], buf.length(), 0, (struct sockaddr *)peeraddr, &peeraddr_size)) == -1)
    {
        PSOCKETERROR("accept error");
        return -1;
    }

    if (Logger::GetGlobalLogLevel() == LogLevel::LLVERBOSE)
    {
        char *ip = inet_ntoa(peeraddr->sin_addr);
        int port = ntohs(peeraddr->sin_port);
        LOGDP("recvfrom(%s:%d)(%d): %s", ip, port, result, buf.substr(0, 64).c_str());
    }
    return result;
}

int join_or_drop(int fd, std::string group_addr, std::string interface_addr, bool join = true)
{
    ip_mreq mreq;
    auto groupaddr = inet_addr(group_addr.c_str());
    auto interfaceaddr = inet_addr(interface_addr.c_str());

    if (IN_MULTICAST(ntohl(groupaddr)) == 0)
        return -1;

    mreq.imr_multiaddr.s_addr = groupaddr;
    mreq.imr_interface.s_addr = interfaceaddr;
    int type = join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;

    if (setsockopt(fd, IPPROTO_IP, type, (const char *)&mreq, sizeof(mreq)) == -1)
    {
        PSOCKETERROREX("setsocketopt %s error", type == IP_ADD_MEMBERSHIP ? "IP_ADD_MEMBERSHIP" : "IP_DROP_MEMBERSHIP");
        return -1;
    }

    LOGDP("multicast group %s: %s(%s)", join ? "join" : "drop", group_addr.c_str(), interface_addr.c_str());
    return 0;
}

/**
 * @brief Used when you need to send multicast data.
 * set IP_MULTICAST_IF and IP_MULTICAST_LOOP
 * 
 * @param interface_addr 
 * @return int 
 */
int Udp::BindMulticastInterface(std::string interface_addr)
{
    int result = 0;
    auto addr = inet_addr(interface_addr.c_str());
    result = setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr));
    ASSERT_RETURN(result >= 0, -1);

    char loopch = 1;
    result = setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch));
    ASSERT_RETURN(result >= 0, -1);

    // char ttl = 1;
    // result = setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
    // ASSERT_RETURN(result >= 0, -1);

    LOGDP("bind multicast interface: %s",interface_addr.c_str());

    return 0;
}

/**
 * @brief Used when you need to recv multicast data.
 * Join multicast group with specific interface.
 * 
 * @param group_addr 
 * @param interface_addr 
 * @return int 
 */
int Udp::JoinMUlticastGroup(std::string group_addr, std::string interface_addr)
{
    return join_or_drop(fd_, group_addr, interface_addr, true);
}

/**
 * @brief Leave a multicast group.
 * 
 * @param group_addr 
 * @param interface_addr 
 * @return int 
 */
int Udp::DropMulticastGroup(std::string group_addr, std::string interface_addr)
{
    return join_or_drop(fd_, group_addr, interface_addr, false);
}