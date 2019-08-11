#pragma once

#include <memory>
#include <functional>
#include <chrono>

#include "sock.h"
#include "command.h"
#include "context2.h"

using namespace std::chrono;

class Peer;
class CommandSender;

class Peer final
{
public:
    Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context)
        : Peer(control_sock, "", context)
    {
    }

    Peer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context)
        : cookie_(cookie), context_(context), control_sock_(control_sock),timeout_(-1)
    {
    }

    inline int GetDataFd() { return data_sock_ ? data_sock_->GetFd() : -1; }

    int SendCommand();
    int RecvCommand();
    int SendData();
    int RecvData();
    int Timeout(int timeout);

    int SendEcho();
    int RecvEcho();

    bool operator==(const Peer &peer)
    {
        return std::addressof(*this) == std::addressof(peer);
    }

    int GetControlFd() { return control_sock_->GetFd(); }
    std::shared_ptr<Sock> GetControlSock(){return control_sock_;}
    std::shared_ptr<Sock> GetDataSock(){return data_sock_;}
    std::shared_ptr<Context> GetContext(){return context_;}
    void SetCommand(std::shared_ptr<Command> command);
    const std::string &GetCookie() { return cookie_; }
    int GetTimeout() { return commandsender_?commandsender_->GetTimeout():-1; }

private:

    int Auth();

    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::string cookie_;
    //std::shared_ptr<Command> command_;
    std::shared_ptr<CommandSender> commandsender_; 
    char buf_[1024 * 64];
    std::shared_ptr<Context> context_;
    int timeout_;

    friend class CommandSender;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};
