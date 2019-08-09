#pragma once

#include <memory>
#include <functional>

#include "sock.h"
#include "command.h"
#include "context2.h"

class Peer final
{
public:
    Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context)
        : Peer(control_sock, "", context)
    {
    }

    Peer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context)
        : cookie_(cookie), context_(context), control_sock_(control_sock), timeout_(0)
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
    std::shared_ptr<Command> GetCmd() { return command_; }
    void SetCommand(std::shared_ptr<Command> command);
    const std::string &GetCookie() { return cookie_; }
    int GetTimeout() { return timeout_; }

protected:
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::string cookie_;
    std::shared_ptr<Command> command_;
    char buf_[1024 * 64];
    std::shared_ptr<Context> context_;
    int timeout_;
    std::function<int()> timeout_callback_;
    int count_;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};

// class EchoPeer
// {
// public:
//     EchoPeer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context)
//         : Peer(control_sock, "", context)
//     {
//     }

//     EchoPeer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context)
//         : Peer(control_sock,cookie,context)
//     {
//     }

//     int SendCommand() override;
//     int RecvCommand() override;
//     int SendData() override;
//     int RecvData() override;
// };