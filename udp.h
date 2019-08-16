#pragma once

#include "sock.h"

class Udp : public Sock
{
public:
    Udp();
    Udp(int fd);
    int Listen(int count) override;
    int Accept() override;
    ssize_t SendTo(const std::string& buf,sockaddr_in* sockaddr);
    ssize_t RecvFrom(std::string& buf,sockaddr_in* sockaddr);

private:

    int count_;
    
    DISALLOW_COPY_AND_ASSIGN(Udp);
};

