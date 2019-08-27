
#include "command.h"

#define REGISER_COMMAND(name, T) \
static CommandRegister<T> __command_##name(#name)
#define REGISER_PRIVATE_COMMAND(name, T) \
static CommandRegister<T> __command_##name(#name,true)


REGISER_COMMAND(ping,EchoCommand);
REGISER_COMMAND(send,SendCommand);
REGISER_PRIVATE_COMMAND(ack,AckCommand);
REGISER_PRIVATE_COMMAND(stop,StopCommand);
REGISER_PRIVATE_COMMAND(result,ResultCommand);


