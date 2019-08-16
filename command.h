#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <functional>

#include "command_receiver.h"
#include "command_sender.h"
#include "netsnoop.h"

#define MAX_CMD_LENGTH 1024

class CommandFactory;
class Command;
class NetStat;

typedef std::function<void(const Command *, std::shared_ptr<NetStat>)> CommandCallback;

enum CommandType : char
{
    CMD_NULL = 0,
    CMD_ECHO = 1,
    CMD_RECV = 2,
    CMD_SEND = 3,
};

extern std::map<std::string, int> g_cmd_map;

using CommandArgs = std::map<std::string, std::string>;
//using Ctor = std::shared_ptr<Command>(*)(std::string);
using Ctor = CommandFactory *;
using CommandContainer = std::map<std::string, Ctor>;

class CommandFactory
{
public:
    static std::shared_ptr<Command> New(const std::string &cmd)
    {
        CommandArgs args;
        std::stringstream ss(cmd);
        std::string name, key, value;
        ss >> name;
        if (Container().find(name) == Container().end())
        {
            LOGE("illegal command: %s\n",cmd.c_str());
            return NULL;
        }
        while (ss >> key)
        {
            if (ss >> value)
            {
                ASSERT(args.find(key) == args.end());
                args[key] = value;
            }
            else
            {
                ASSERT(!value.empty());
                args[key] = "";
            }
        }
        return Container()[name]->NewCommand(cmd, args);
    }

protected:
    static CommandContainer &Container()
    {
        static CommandContainer commands;
        return commands;
    }
    virtual std::shared_ptr<Command> NewCommand(const std::string &cmd, CommandArgs args) = 0;
};
template <class DerivedType>
class CommandRegister : public CommandFactory
{
public:
    CommandRegister(const std::string &name) : CommandRegister(name, false) {}
    CommandRegister(const std::string &name, bool is_private) : name_(name), is_private_(is_private)
    {
        ASSERT(Container().find(name) == Container().end());
        LOGV("register command: %s\n", name.c_str());
        Container()[name] = this;
    }
    std::shared_ptr<Command> NewCommand(const std::string &cmd, CommandArgs args) override
    {
        auto command = std::make_shared<DerivedType>(cmd);
        command->is_private = is_private_;
        if (command->ResolveArgs(args))
        {
            LOGV("new command: %s:%s\n", name_.c_str(), cmd.c_str());
            return command;
        }
        LOGV("new command error: %s:%s\n", name_.c_str(), cmd.c_str());
        return NULL;
    }

private:
    const std::string name_;
    bool is_private_;
};

/**
 * @brief Network State
 * 
 */
struct NetStat
{
    /**
     * @brief Network delay in millseconds
     * 
     */
    int delay;
    int max_delay;
    int min_delay;
    /**
     * @brief Jitter in millseconds
     * 
     */
    int jitter;
    /**
     * @brief Packet loss percent
     * 
     */
    double loss;

    /**
     * @brief Send/Recv packets count
     * 
     */
    long long send_packets;
    long long recv_packets;

    /**
     * @brief Send/Recv data length
     * 
     */
    long long send_bytes;
    long long recv_bytes;

    /**
     * @brief Command send/recv time in millseconds
     * 
     */
    int send_time;
    int recv_time;
    /**
     * @brief Send speed in Byte/s
     * 
     */
    long long send_speed;
    long long min_send_speed;
    long long max_send_speed;

    long long recv_speed;
    long long min_recv_speed;
    long long max_recv_speed;

    double errors;
    int retransmits;

    void FromCommandArgs(CommandArgs &args)
    {
#define RI(p) p = atoi(args[#p].c_str())
#define RLL(p) p = atoll(args[#p].c_str())
#define RF(p) p = atof(args[#p].c_str())

        RI(delay);
        RI(min_delay);
        RI(max_delay);
        RI(jitter);
        RF(loss);
        RLL(send_packets);
        RLL(send_bytes);
        RLL(recv_packets);
        RLL(recv_bytes);
        RI(send_time);
        RI(recv_time);
        RLL(send_speed);
        RLL(max_send_speed);
        RLL(min_send_speed);
        RLL(recv_speed);
        RLL(max_recv_speed);
        RLL(min_recv_speed);
#undef RI
#undef RLL
#undef RF
    }

    std::string ToString() const
    {
        std::stringstream ss;
#define W(p)   \
    if (p > 0) \
    ss << #p " " << p << " "
        W(loss);
        W(send_speed);
        W(max_send_speed);
        W(min_send_speed);
        W(recv_speed);
        W(max_recv_speed);
        W(min_recv_speed);
        W(delay);
        W(min_delay);
        W(max_delay);
        W(jitter);
        W(send_packets);
        W(send_bytes);
        W(recv_packets);
        W(recv_bytes);
        W(send_time);
        W(recv_time);
#undef W
        return ss.str();
    }

    NetStat& operator+=(const NetStat& stat)
    {
        #define AI(p) p=(p+stat.p+1)/2
        #define AF(p) p=(p+stat.p)/2
        #define A(p)  p=p+stat.p
        AF(loss);
        A(send_speed);
        A(max_send_speed);
        A(min_send_speed);
        A(recv_speed);
        A(max_recv_speed);
        A(min_recv_speed);
        AI(delay);
        AI(min_delay);
        AI(max_delay);
        AI(jitter);
        A(send_packets);
        A(send_bytes);
        A(recv_packets);
        A(recv_bytes);
        A(send_time);
        A(recv_time);
        #undef AI
        #undef AF
        return *this;
    }

};

struct CommandChannel
{
    std::shared_ptr<Command> command_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
};

/**
 * @brief A command stands for a type of network test.
 * 
 */
class Command
{
public:
    Command(std::string name, std::string cmd) : name(name), cmd(cmd), is_private(false) {}
    void RegisterCallback(CommandCallback callback)
    {
        if (callback)
            callbacks_.push_back(callback);
    }

    void InvokeCallback(std::shared_ptr<NetStat> netstat)
    {
        for (auto &callback : callbacks_)
        {
            callback(this, netstat);
        }
    }

    virtual bool ResolveArgs(CommandArgs args) { return true; };
    virtual std::shared_ptr<CommandSender> CreateCommandSender(std::shared_ptr<CommandChannel> channel) { return NULL; };
    virtual std::shared_ptr<CommandReceiver> CreateCommandReceiver(std::shared_ptr<CommandChannel> channel) { return NULL; };

    std::string name;
    std::string cmd;
    bool is_private;

private:
    std::vector<CommandCallback> callbacks_;

    DISALLOW_COPY_AND_ASSIGN(Command);
};

#define ECHO_DEFAULT_COUNT 5
#define ECHO_DEFAULT_INTERVAL 200
#define ECHO_DEFAULT_TIME 1
#define ECHO_DEFAULT_SIZE 32
#define ECHO_DEFAULT_SPEED 0

class EchoCommand : public Command
{
public:
    // format: echo [count <num>] [time <num>] [interval <num>] [size <num>]
    // example: echo count 10 interval 100
    EchoCommand(std::string cmd)
        : count_(ECHO_DEFAULT_COUNT),
          time_(ECHO_DEFAULT_TIME),
          interval_(ECHO_DEFAULT_INTERVAL),
          size_(ECHO_DEFAULT_SIZE),
          speed_(0),
          Command("echo", cmd)
    {
    }
    bool ResolveArgs(CommandArgs args) override
    {
        try
        {
            count_ = args["count"].empty() ? ECHO_DEFAULT_COUNT : std::stoi(args["count"]);
            interval_ = args["interval"].empty() ? ECHO_DEFAULT_INTERVAL : std::stoi(args["interval"]);
            size_ = args["size"].empty() ? ECHO_DEFAULT_SIZE : std::stoi(args["size"]);
            time_ = args["time"].empty() ? ECHO_DEFAULT_TIME : std::stoi(args["time"]);
            speed_ = args["speed"].empty() ? ECHO_DEFAULT_SPEED : std::stoi(args["speed"]);
            // echo can not have zero delay
            if(interval_<=0) interval_ = ECHO_DEFAULT_INTERVAL;
            return true;
        }
        catch (const std::exception &e)
        {
            LOGE("EchoCommand resolve args error: %s\n", e.what());
        }
        return false;
    }

    std::shared_ptr<CommandSender> CreateCommandSender(std::shared_ptr<CommandChannel> channel) override
    {
        return std::make_shared<EchoCommandSender>(channel);
    }
    std::shared_ptr<CommandReceiver> CreateCommandReceiver(std::shared_ptr<CommandChannel> channel) override
    {
        return std::make_shared<EchoCommandReceiver>(channel);
    }

    int GetCount() { return count_; }
    int GetInterval() { return interval_; }
    int GetTime() { return time_; }
    int GetSize() { return size_; }

private:
    int count_;
    int time_;
    int interval_;
    int size_;
    int speed_;

    DISALLOW_COPY_AND_ASSIGN(EchoCommand);
};

#define RECV_DEFAULT_COUNT 10
#define RECV_DEFAULT_INTERVAL 100
#define RECV_DEFAULT_TIME 1
#define RECV_DEFAULT_SIZE 1024 * 10
#define RECV_DEFAULT_SPEED 0
class RecvCommand : public Command
{
public:
    RecvCommand(std::string cmd) : Command("recv", cmd) {}

    bool ResolveArgs(CommandArgs args) override
    {
        try
        {
            count_ = args["count"].empty() ? RECV_DEFAULT_COUNT : std::stoi(args["count"]);
            interval_ = args["interval"].empty() ? RECV_DEFAULT_INTERVAL : std::stoi(args["interval"]);
            size_ = args["size"].empty() ? RECV_DEFAULT_SIZE : std::stoi(args["size"]);
            time_ = args["time"].empty() ? RECV_DEFAULT_TIME : std::stoi(args["time"]);
            speed_ = args["speed"].empty() ? RECV_DEFAULT_SPEED : std::stoi(args["speed"]);
            return true;
        }
        catch (const std::exception &e)
        {
            LOGE("RecvCommand resolve args error: %s\n", e.what());
        }
        return false;
    }

    std::shared_ptr<CommandSender> CreateCommandSender(std::shared_ptr<CommandChannel> channel) override
    {
        return std::make_shared<RecvCommandSender>(channel);
    }
    std::shared_ptr<CommandReceiver> CreateCommandReceiver(std::shared_ptr<CommandChannel> channel) override
    {
        return std::make_shared<RecvCommandReceiver>(channel);
    }

    int GetCount() { return count_; }
    int GetInterval() { return interval_; }
    int GetTime() { return time_; }
    int GetSize() { return size_; }

private:
    int count_;
    int interval_;
    int time_;
    int size_;
    int speed_;

    DISALLOW_COPY_AND_ASSIGN(RecvCommand);
};

// #define DEFINE_COMMAND(name,typename) \
// class typename : public Command \
// {\
// public:\
//     typename(std::string cmd):Command(#name,cmd){}\
// }

class StopCommand : public Command
{
public:
    StopCommand() : StopCommand("stop") {}
    StopCommand(std::string cmd) : Command("stop", cmd) {}

    DISALLOW_COPY_AND_ASSIGN(StopCommand);
};

class ResultCommand : public Command
{
public:
    ResultCommand() : ResultCommand("result") {}
    ResultCommand(std::string cmd) : Command("result", cmd) {}
    bool ResolveArgs(CommandArgs args) override
    {
        netstat = std::make_shared<NetStat>();
        netstat->FromCommandArgs(args);
        return true;
    }
    std::string Serialize(const NetStat &netstat)
    {
        return name + " " + netstat.ToString();
    }

    std::shared_ptr<NetStat> netstat;

    DISALLOW_COPY_AND_ASSIGN(ResultCommand);
};