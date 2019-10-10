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

class Peer
{
public:
    Peer(std::shared_ptr<Sock> control_sock,std::shared_ptr<Option> option, std::shared_ptr<Context> context);

    int Start();
    int Stop();

    int SendCommand();
    int RecvCommand();
    int SendData();
    int RecvData();
    int Timeout(int timeout);
    
    int GetControlFd() const{ return control_sock_->GetFd(); }
    int GetDataFd() const;
    std::shared_ptr<Sock> GetControlSock() const{return control_sock_;}
    std::shared_ptr<Sock> GetDataSock() const{return data_sock_;}
    std::shared_ptr<Context> GetContext() const{return context_;}
    int SetCommand(std::shared_ptr<Command> command);
    std::shared_ptr<Command> GetCommand() const{return command_;};
    const std::string &GetCookie() const { return cookie_; }
    int GetTimeout() const{ return commandsender_?commandsender_->GetTimeout():-1; }
    bool IsReady() const{return !!data_sock_tcp_;}
    bool IsPayloadStarted() const {return commandsender_&&commandsender_->is_started_;}

    std::function<void(const Peer*,std::shared_ptr<NetStat>)> OnStopped;
    std::function<void(const Peer*)> OnAuthSuccess;

    bool operator==(const Peer &peer)
    {
        return std::addressof(*this) == std::addressof(peer);
    }

    // TODO: optimize multicast logic
    std::shared_ptr<Sock> multicast_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<Sock> data_sock_tcp_;
    std::string cookie_;

private:
    std::shared_ptr<Option> option_;
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> current_sock_;
    std::shared_ptr<Command> command_;
    std::shared_ptr<CommandSender> commandsender_; 
    char buf_[MAX_UDP_LENGTH];
    std::shared_ptr<Context> context_;

    friend class CommandSender;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};