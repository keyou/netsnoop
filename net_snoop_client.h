#pragma once

#include <memory>
#include <sys/un.h>
#include <queue>

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
    int RecvCommand();
    int SendCommand();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<CommandReceiver> receiver_;

    std::queue<std::shared_ptr<Command>> commands_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopClient);
};