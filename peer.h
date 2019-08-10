#pragma once

#include <memory>
#include <functional>
#include <chrono>

#include "sock.h"
#include "command.h"
#include "context2.h"

using namespace std::chrono;

class Peer;
class Mode;

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
    void SetCommand(std::shared_ptr<Command> commjnd);
    const std::string &GetCookie() { return cookie_; }
    int GetTimeout() { return timeout_; }

private:

    int Auth();

    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::string cookie_;
    //std::shared_ptr<Command> command_;
    std::shared_ptr<Mode> mode_; 
    char buf_[1024 * 64];
    std::shared_ptr<Context> context_;
    int timeout_;

    friend class Mode;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};

class Mode
{
public:
    Mode(Peer* peer,std::shared_ptr<Command> command)
        : peer_(peer),control_sock_(peer->control_sock_),data_sock_(peer->data_sock_),context_(peer->context_),command_(command)
    {
    }

    virtual int SendCommand();
    virtual int RecvCommand(){return 0;};
    virtual int SendData(){return 0;};
    virtual int RecvData(){return 0;};
    virtual int OnTimeout(){return 0;};

    void SetTimeout(int timeout){peer_->timeout_ = timeout;}

protected:
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<Command> command_;
    std::shared_ptr<Context> context_;
    Peer* peer_;

    DISALLOW_COPY_AND_ASSIGN(Mode);
};

class EchoMode : public Mode
{
public:
    EchoMode(Peer* peer,std::shared_ptr<Command> command)
        : Mode(peer,command),buf_{0},count_(0)
    {
    }

    int SendCommand() override;
    int RecvCommand() override;
    int SendData() override;
    int RecvData() override;
    int OnTimeout() override;
    
private:
    high_resolution_clock::time_point start_;
    high_resolution_clock::time_point stop_;
    high_resolution_clock::time_point begin_;
    high_resolution_clock::time_point end_;

    char buf_[1024 * 64];
    int count_;
};

class RecvMode : public Mode
{
public:
    RecvMode(Peer* peer,std::shared_ptr<Command> command)
        : Mode(peer,command)
    {
    }
    int SendCommand() override;
    int RecvCommand() override;
    int SendData() override;
    int RecvData() override;
    int OnTimeout() override;
private:
    std::string buf_;
    int count_;
};

