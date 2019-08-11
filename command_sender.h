#pragma once

#include <memory>
#include <functional>
#include <chrono>

#include "sock.h"
#include "context2.h"

using namespace std::chrono;

class Peer;
class Command;
class CommandChannel;

class CommandSender
{
public:
    CommandSender(std::shared_ptr<CommandChannel> channel);

    virtual int SendCommand();
    virtual int RecvCommand() { return 0; };
    virtual int SendData() { return 0; };
    virtual int RecvData() { return 0; };
    virtual int OnTimeout() { return 0; };

    void SetTimeout(int timeout) { timeout_ = timeout; };
    int GetTimeout() { return timeout_; };

protected:
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Command> command_;
    int timeout_;

    DISALLOW_COPY_AND_ASSIGN(CommandSender);
};

class EchoCommandSender : public CommandSender
{
public:
    EchoCommandSender(std::shared_ptr<CommandChannel> channel)
        : CommandSender(channel), buf_{0}, count_(0)
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

class RecvCommandSender : public CommandSender
{
public:
    RecvCommandSender(std::shared_ptr<CommandChannel> channel)
        : CommandSender(channel)
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
