#pragma once

#include <chrono>
#include <queue>

#include "context2.h"
#include "sock.h"

using namespace std::chrono;

class Command;
class CommandChannel;
class EchoCommand;
class SendCommand;
class NetStat;

class CommandReceiver
{
public:
    CommandReceiver(std::shared_ptr<CommandChannel> channel);

    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int Send() { ASSERT(0);return -1; }
    virtual int Recv() { ASSERT(0);return -1; }
    virtual int RecvPrivateCommand(std::shared_ptr<Command> private_command);
    virtual int SendPrivateCommand(){return 0;}

    std::function<void(std::shared_ptr<Command>,std::shared_ptr<NetStat>)> OnStopped;

    int GetDataFd(){return data_sock_?data_sock_->GetFd():-1;}

protected:
    std::string argv_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
};

class EchoCommandReceiver : public CommandReceiver
{
public:
    EchoCommandReceiver(std::shared_ptr<CommandChannel> channel);

    int Start() override;
    int Stop() override;
    int Send() override;
    int Recv() override;
    int SendPrivateCommand() override;

private:
    ssize_t recv_count_;
    ssize_t send_count_;
    bool running_;
    bool is_stopping_;
    std::shared_ptr<EchoCommand> command_;
    std::queue<std::string> data_queue_;
};

class SendCommandReceiver : public CommandReceiver
{
public:
    SendCommandReceiver(std::shared_ptr<CommandChannel> channel);

    int Start() override;
    int Stop() override;
    int Recv() override;
    int SendPrivateCommand() override;

private:
    char buf_[MAX_UDP_LENGTH];
    int length_;
    bool running_;
    bool is_stopping_;
    std::shared_ptr<SendCommand> command_;

    high_resolution_clock::time_point start_;
    high_resolution_clock::time_point stop_;
    high_resolution_clock::time_point begin_;
    high_resolution_clock::time_point end_;

    ssize_t recv_count_;
    int64_t recv_bytes_;
    int64_t speed_;
    int64_t max_speed_;
    int64_t min_speed_;

};
