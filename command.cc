
#include "command.h"

std::map<std::string,int> g_cmd_map = {
    {"echo",CMD_ECHO},{"recv",CMD_RECV},{"send",CMD_SEND}
};

Command::Command(const std::string &cmd)
    : cmd(cmd)
{
    std::stringstream ss(cmd);
    std::string arg;
    ss >> this->name;
    while (ss >> arg)
    {
        args.push_back(arg);
    }
    id = g_cmd_map[this->name];
}

//static
std::shared_ptr<Command> Command::CreateCommand(const std::string &cmd)
{
    if (Command(cmd).id <= 0)
        return NULL;
    return std::make_shared<Command>(cmd);
}

//static
std::string Command::GetCommandName(std::string cmd)
{
    std::stringstream ss(cmd);
    std::string name;
    ss >> name;
    return name;
}
