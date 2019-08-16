#pragma once

#include "sock.h"

class Tcp : public Sock
{
public:
    Tcp();
    Tcp(int fd);
    int Listen(int count) override;
    int Accept() override;

private:
    int InitializeEx(int fd) const override;
    
    DISALLOW_COPY_AND_ASSIGN(Tcp);
};