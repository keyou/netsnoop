#pragma once

#include <memory>
#include <sys/un.h>

#include "tcp.h"
#include "udp.h"
#include "action.h"

class NetSnoopClient
{
public:
    NetSnoopClient(std::shared_ptr<Option> option)
        : option_(option)
    {
    }
    int Run();

private:
    int Connect();
    int ParseAction(std::shared_ptr<Action> &action);

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> tcp_client_;
    std::shared_ptr<Udp> udp_client_;
    std::shared_ptr<Action> action_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopClient);
};