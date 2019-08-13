#pragma once

#include <list>
#include <queue>
#include <mutex>

#include "command.h"
#include "tcp.h"
#include "udp.h"
#include "peer.h"

class NetSnoopServer
{
public:
    NetSnoopServer(std::shared_ptr<Option> option)
        :option_(option),
        context_(std::make_shared<Context>()),
        listen_tcp_(std::make_shared<Tcp>()),
        is_running_(false)
        {}
    int Run();
    int PushCommand(std::shared_ptr<Command> command);

    std::function<void(Peer* peer)> OnAcceptNewPeer;
    std::function<void(NetSnoopServer* server)> OnServerStart;

private:
    int StartListen();
    int AceeptNewConnect();
    int AcceptNewCommand();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> listen_tcp_;
    std::list<std::shared_ptr<Peer>> peers_;
    int pipefd_[2];
    std::queue<std::shared_ptr<Command>> commands_;
    bool is_running_;
    std::mutex mtx;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopServer);
};
