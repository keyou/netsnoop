#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <functional>

#include "netsnoop.h"

#define MAX_CMD_LENGTH 100

class CommandFactory;
class Command;
class NetStat;

typedef std::function<void(Command *, std::shared_ptr<NetStat>)> CommandCallback;

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
            return NULL;
        while (ss >> key)
        {
            if (ss >> value)
            {
                args[key] = value;
                continue;
            }
            // Params are not pair.
            return NULL;
        }
        return Container()[name]->NewCommand(cmd, args);
    }

protected:
    virtual std::shared_ptr<Command> NewCommand(const std::string &cmd, CommandArgs args) = 0;
    static CommandContainer &Container()
    {
        static CommandContainer commands;
        return commands;
    }
};
template <typename T>
class CommandRegister : public CommandFactory
{
public:
    CommandRegister(const std::string &name) : name_(name)
    {
        ASSERT(Container().find(name) == Container().end());
        LOGV("register command: %s\n", name.c_str());
        Container()[name] = this;
    }
    std::shared_ptr<Command> NewCommand(const std::string &cmd, CommandArgs args) override
    {
        auto command = std::make_shared<T>();
        command->name = name_;
        command->cmd = cmd;
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
     * @brief Packet count
     * 
     */
    int packets;

    /**
     * @brief Total data length
     * 
     */
    long long bytes;

    /**
     * @brief Transport speed in Byte/s
     * 
     */
    int speed;

    double errors;
    int retransmits;
};

/**
 * @brief A command stands for a type of network test.
 * 
 */
class Command
{
public:
    void RegisterCallback(CommandCallback callback)
    {
        if (callback)
            callbacks_.push_back(callback);
    }

    void InvokeCallback(std::shared_ptr<NetStat> netstat)
    {
        for (auto callback : callbacks_)
        {
            callback(this, netstat);
        }
    }

    virtual bool ResolveArgs(CommandArgs args) = 0;

    int id;
    std::string name;
    std::string cmd;

private:
    CommandArgs args;
    std::vector<CommandCallback> callbacks_;
};

class EchoCommand : public Command
{
public:
#define ECHO_DEFAULT_COUNT 10
#define ECHO_DEFAULT_INTERVAL 100
#define ECHO_DEFAULT_TIME 1
#define ECHO_DEFAULT_SIZE 32

    // format: echo [count <num>] [time <num>] [interval <num>] [size <num>]
    // example: echo count 10 interval 100
    EchoCommand()
        : count_(ECHO_DEFAULT_COUNT),
          time_(ECHO_DEFAULT_TIME),
          interval_(ECHO_DEFAULT_INTERVAL),
          size_(ECHO_DEFAULT_SIZE)
    {
    }

    bool ResolveArgs(CommandArgs args) override
    {
        try
        {
            count_ = args["count"].empty() ? ECHO_DEFAULT_COUNT : std::stoi(args["count"]);
            time_ = args["time"].empty() ? ECHO_DEFAULT_TIME : std::stoi(args["time"]);
            interval_ = args["interval"].empty() ? ECHO_DEFAULT_INTERVAL : std::stoi(args["interval"]);
            size_ = args["size"].empty() ? ECHO_DEFAULT_SIZE : std::stoi(args["size"]);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr <<"echo resolve args error:"<< e.what() << '\n';
        }
        return false;
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
};

class RecvCommand : public Command
{
public:
    RecvCommand() {}

    bool ResolveArgs(CommandArgs args) override
    {
        return true;
    }

    int GetCount()
    {
        return 10;
    }

private:
    int count_;
    int time_;
    int interval_;
    int size_;
    int speed_;
};
