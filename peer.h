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
    Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context);
    Peer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context);

    int Start();
    int Stop();

    int SendCommand();
    int RecvCommand();
    int SendData();
    int RecvData();
    int Timeout(int timeout);
    
    int GetControlFd() const{ return control_sock_->GetFd(); }
    int GetDataFd() const{ return data_sock_ ? data_sock_->GetFd() : -1; }
    std::shared_ptr<Sock> GetControlSock() const{return control_sock_;}
    std::shared_ptr<Sock> GetDataSock() const{return data_sock_;}
    std::shared_ptr<Context> GetContext() const{return context_;}
    int SetCommand(std::shared_ptr<Command> command);
    std::shared_ptr<Command> GetCommand() const{return command_;};
    const std::string &GetCookie() { return cookie_; }
    int GetTimeout() const{ return commandsender_?commandsender_->GetTimeout():-1; }
    bool IsReady() const{return !!data_sock_;}

    std::function<void(const Peer*,std::shared_ptr<NetStat>)> OnStopped;
    std::function<void(const Peer*)> OnAuthSuccess;

    bool operator==(const Peer &peer)
    {
        return std::addressof(*this) == std::addressof(peer);
    }

private:

    int Auth();

    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::string cookie_;
    std::shared_ptr<Command> command_;
    std::shared_ptr<CommandSender> commandsender_; 
    char buf_[1024 * 64];
    std::shared_ptr<Context> context_;

    friend class CommandSender;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};
