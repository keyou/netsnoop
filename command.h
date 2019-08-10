#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include <functional>

#define MAX_CMD_LENGTH  100 

class Command;
class NetStat;

typedef std::function<void(Command*,std::shared_ptr<NetStat>)> CommandCallback;

enum CommandType : char
{
    CMD_NULL = 0,
    CMD_ECHO = 1,
    CMD_RECV = 2,
    CMD_SEND = 3,
};

extern std::map<std::string,int> g_cmd_map;

/**
 * @brief Network State
 * 
 */
struct NetStat
{
    int delay;
    int jitter;
    double loss;
    double errors;
    int retransmits;
    int packets;
    long long bytes;
};




/**
 * @brief A command stands for a type of network test.
 * 
 */
class Command
{
public:
    Command(const std::string& cmd);

    static std::shared_ptr<Command> CreateCommand(const std::string& cmd);

    static std::string GetCommandName(std::string cmd);

    void RegisterCallback(CommandCallback callback)
    {
        if(callback) callbacks_.push_back(callback);
    }
    
    void InvokeCallback(std::shared_ptr<NetStat> netstat)
    {
        for(auto callback : callbacks_)
        {
            callback(this,netstat);
        }
    }

    std::string cmd;
    std::string name;
    int id;
    std::vector<std::string> args;
private:
    std::vector<CommandCallback> callbacks_;
};

class EchoCommand: public Command
{
public:
    // format: echo [count] [interval in ms]
    // example: echo 10 100
    EchoCommand(const std::string& cmd):Command(cmd){}

    int GetCount()
    {
        return args.size()>0?atoi(args[0].c_str()):10;
    }

    int GetInterval()
    {
        return args.size()>1?atoi(args[1].c_str()):100;
    }
};

class RecvCommand
{
public:
   RecvCommand();
    ~RecvCommand();
};

RecvCommand::RecvCommand()
{
}

RecvCommand::~RecvCommand()
{
}

