#pragma once

#include <memory>
#include <sys/un.h>

#include "tcp.h"
#include "udp.h"
#include "command_receiver.h"

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
    int ReceiveCommand();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> tcp_client_;
    std::shared_ptr<Udp> udp_client_;
    std::shared_ptr<CommandReceiver> receiver_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopClient);
};