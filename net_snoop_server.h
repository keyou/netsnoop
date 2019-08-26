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
        is_running_(false),is_multicast_started_(false)
        {}
    /**
     * @brief Start the server, it will stuck here.
     *  There is no any method to stop a server unless shutdown the process.
     * 
     * @return int 
     */
    int Run();

    /**
     * @brief Push a new command.
     * 
     * @param command 
     * @return int 
     */
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
    /**
     * @brief The socket to accept clients.
     * 
     */
    std::shared_ptr<Sock> listen_peers_sock_;
    /**
     * @brief The socket to read test command.
     * 
     */
    std::shared_ptr<Udp> command_sock_read_;
    /**
     * @brief The socket to wake select up to begin a new command.
     * 
     */
    std::shared_ptr<Udp> command_sock_write_;
    /**
     * @brief The socket for multicast testing.
     * 
     */
    std::shared_ptr<Sock> multicast_sock_;

    /**
     * @brief All connected peers, no matter auth or not.
     * 
     */
    std::list<std::shared_ptr<Peer>> peers_;
    /**
     * @brief The authed peers.
     * 
     */
    std::list<std::shared_ptr<Peer>> ready_peers_;
    std::queue<std::shared_ptr<Command>> commands_;
    std::shared_ptr<Command> current_command_;
    std::shared_ptr<NetStat> netstat_;
    /**
     * @brief To sync commands read and write.
     * 
     */
    std::mutex mtx;
    //int pipefd_[2];

    bool is_running_;
    bool is_multicast_started_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopServer);
};
