#pragma once

#include <list>
#include <queue>
#include <mutex>

#include "command.h"
#include "tcp.h"
#include "udp.h"
#include "peer.h"

#define CMD_ILLEGAL "command illegal."

class NetSnoopServer
{
public:
    NetSnoopServer(std::shared_ptr<Option> option)
        :option_(option),
        context_(std::make_shared<Context>()),
        listen_peers_sock_(std::make_shared<Tcp>()),
        multicast_sock_(std::make_shared<Udp>()),
        is_running_(false)
        {}
    int Run();
    int PushCommand(std::shared_ptr<Command> command);

    std::function<void(const NetSnoopServer* server)> OnServerStart;
    std::function<void(const Peer*)> OnPeerConnected;
    std::function<void(const Peer*)> OnPeerDisconnected;
    std::function<void(const Peer*,std::shared_ptr<NetStat>)> OnPeerStopped;

private:
    int StartListen();
    int AceeptNewConnect();
    int AcceptNewCommand();
    int ProcessNextCommand();

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Sock> listen_peers_sock_;
    //std::shared_ptr<Udp> command_sock_;
    std::shared_ptr<Sock> multicast_sock_;
    std::list<std::shared_ptr<Peer>> peers_;
    std::queue<std::shared_ptr<Command>> commands_;
    std::list<std::shared_ptr<Peer>> ready_peers_;
    std::shared_ptr<NetStat> netstat_;
    bool is_running_;
    std::mutex mtx;
    int pipefd_[2];

    DISALLOW_COPY_AND_ASSIGN(NetSnoopServer);
};
