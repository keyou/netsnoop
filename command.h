#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sstream>

enum CommandType : char
{
    CMD_NULL = 0,
    CMD_ECHO = 1,
    CMD_RECV = 2,
    CMD_SEND = 3,
};

extern std::map<std::string,int> g_cmd_map;

struct Command
{
    Command(const std::string& cmd);

    static std::shared_ptr<Command> CreateCommand(const std::string& cmd);

    static std::string GetCommandName(std::string cmd);

    std::string cmd;
    std::string name;
    int id;
    std::vector<std::string> args;
};

struct EchoCommand: public Command
{
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