
#include "command.h"

#define REGISER_COMMAND(name, T) \
static CommandRegister<T> __command_##name(#name)


REGISER_COMMAND(echo,EchoCommand);
REGISER_COMMAND(recv,RecvCommand);


