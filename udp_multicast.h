#pragma once

#include <vector>

#include "udp.h"

class Multicast : public Udp
{
public:
    Multicast(std::string interface_address = "0.0.0.0")
        : interface_address_(interface_address)
    {
    }
    int InitializeEx(int fd_) const override
    {
        Udp::InitializeEx(fd_);
        int result = 0;
        auto addr = inet_addr(interface_address_.c_str());
        result = setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr));
        ASSERT_RETURN(result >= 0, -1);

        char loopch = 1;
        result = setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch));
        ASSERT_RETURN(result >= 0, -1);

        return 0;
    }

private:
    std::vector<std::string> Host2Ips(const std::string &host)
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

    std::string interface_address_;
};