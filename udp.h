#pragma once

#include "sock.h"

class Udp : public Sock
{
public:
    Udp();
    Udp(int fd);
    int Listen(int count) override;
    int Accept() override;

private:
    static int Connect(int fd,sockaddr_in* remoteaddr,int size);

    int count_;
    
    DISALLOW_COPY_AND_ASSIGN(Udp);
};

