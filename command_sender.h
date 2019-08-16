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
class EchoCommand;
class RecvCommand;
class NetStat;

using RecvCommandClazz = class RecvCommand;

class CommandSender
{
public:
    CommandSender(std::shared_ptr<CommandChannel> channel);

    int Start();
    int Stop();
    int SendCommand();
    int RecvCommand();
    virtual int SendData() { return 0; };
    virtual int RecvData() { return 0; };

    int Timeout(int timeout);

    void SetTimeout(int timeout) { timeout_ = timeout>0?timeout:-1; };
    int GetTimeout() { return timeout_; };

    std::function<void(std::shared_ptr<NetStat>)> OnStopped;

protected:
    virtual int OnSendCommand();
    virtual int OnRecvCommand(std::shared_ptr<Command> command);
    virtual int OnStart();
    virtual int OnStop(std::shared_ptr<NetStat> result_command);
    virtual int OnTimeout() { return 0; };
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<Context> context_;

private:
    int timeout_;
    std::shared_ptr<Command> command_;
    bool is_stopping_;
    bool is_stopped_;
    bool is_starting_;
    bool is_started_;
    bool is_waiting_result_;
    bool is_waiting_ack_;

    DISALLOW_COPY_AND_ASSIGN(CommandSender);
};

class EchoCommandSender : public CommandSender
{
public:
    EchoCommandSender(std::shared_ptr<CommandChannel> channel);

    int SendData() override;
    int RecvData() override;
    int OnTimeout() override;

private:
    int OnStart() override;
    int OnStop(std::shared_ptr<NetStat> netstat) override;

    std::shared_ptr<EchoCommand> command_;

    high_resolution_clock::time_point start_;
    high_resolution_clock::time_point stop_;
    high_resolution_clock::time_point begin_;
    high_resolution_clock::time_point end_;

    int64_t delay_;
    int64_t min_delay_;
    int64_t max_delay_;
    ssize_t send_count_;
    ssize_t recv_count_;
    std::string data_buf_;
};

class RecvCommandSender : public CommandSender
{
public:
    RecvCommandSender(std::shared_ptr<CommandChannel> channel);
    
    int SendData() override;
    int RecvData() override;
    int OnTimeout() override;

private:
    int OnStart() override;
    int OnStop(std::shared_ptr<NetStat> netstat) override;
    inline bool TryStop();

    std::shared_ptr<RecvCommandClazz> command_;
    bool is_stoping_;

    high_resolution_clock::time_point start_;
    high_resolution_clock::time_point stop_;
    high_resolution_clock::time_point begin_;
    high_resolution_clock::time_point end_;

    double delay_;
    double min_delay_;
    double max_delay_;
    ssize_t send_count_;
    ssize_t send_bytes_;
    std::string data_buf_;
};
