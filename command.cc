
#include "command.h"

#define REGISER_COMMAND(name, T) \
static CommandRegister<T> __command_##name(#name)

// template <typename T>
// std::shared_ptr<Command> NewCommand(CommandArgs args)
// {
//     auto command = std::make_shared<T>();
//     if(command->ResolveArgs(args)) return command;
//     return NULL;
// }

//static CommandRegister<EchoCommand> e("echo");

REGISER_COMMAND(echo,EchoCommand);
REGISER_COMMAND(recv,RecvCommand);

// std::map<std::string, int> g_cmd_map = {
//     {"echo", CMD_ECHO}, {"recv", CMD_RECV}, {"send", CMD_SEND}};

// std::map<std::string, Ctor> CommandFactory::s_commands;

