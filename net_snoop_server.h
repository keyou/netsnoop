#pragma once

#include <chrono>

#include "command.h"
#include "tcp.h"
#include "udp.h"
#include "peer.h"

using namespace std::chrono;

class NetSnoopServer
{
public:
    NetSnoopServer(std::shared_ptr<Option> option)
        :option_(option),
        context_(std::make_shared<Context>()),
        listen_tcp_(std::make_shared<Tcp>())
        //listen_udp_(std::make_shared<Udp>())
        {}
    int Run();
    int SendCommand(std::string cmd);

private:
    int StartListen();
    int AceeptNewPeer();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> listen_tcp_;
    std::vector<std::shared_ptr<Peer>> peers_;
    int pipefd_[2];
    //std::shared_ptr<Udp> listen_udp_;
    
    //std::vector<int> half_connect_data_fds_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopServer);
};