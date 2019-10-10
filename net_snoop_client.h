#pragma once

#include <memory>
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

    std::function<void(std::shared_ptr<Command>,std::shared_ptr<NetStat>)> OnStopped;
    std::function<void()> OnConnected;

private:
    int Connect();
    int RecvCommand();
    int SendCommand();
    int RecvData(std::shared_ptr<Sock> data_sock);
    int SendData();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<Sock> data_sock_tcp_;
    std::shared_ptr<Udp> multicast_sock_;
    std::shared_ptr<CommandReceiver> receiver_;

    ssize_t illegal_packets_ = 0;
    std::string cookie_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopClient);
};