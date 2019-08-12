
#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "command_sender.h"
#include "context2.h"
#include "peer.h"

int Peer::SendCommand()
{
    ASSERT(commandsender_);
    commandsender_->SendCommand();
    // Stop send control cmd and Start recv control cmd.
    context_->ClrWriteFd(control_sock_->GetFd());
    context_->SetReadFd(control_sock_->GetFd());
    return 0;
}
int Peer::RecvCommand()
{
    if (cookie_.empty())
    {
        return Auth();
    }
    if (!commandsender_)
    {
        LOGE("illegal data.\n");
        return -1;
    }

    return commandsender_->RecvCommand();
}

int Peer::SendData()
{
    ASSERT(commandsender_);
    return commandsender_->SendData();
}

int Peer::RecvData()
{
    ASSERT(commandsender_);
    return commandsender_->RecvData();
}

int Peer::Auth()
{
    int result;
    std::string buf(1024, '\0');
    if ((result = control_sock_->Recv(&buf[0], buf.length())) <= 0)
    {
        LOGE("Disconnect.\n");
        context_->ClrReadFd(control_sock_->GetFd());
        context_->ClrReadFd(data_sock_->GetFd());
        context_->ClrWriteFd(control_sock_->GetFd());
        context_->ClrWriteFd(data_sock_->GetFd());
        return -1;
    }
    buf.resize(result);

    if (buf.rfind("cookie:", 0) != 0)
    {
        LOGE("Bad client.\n");
        return -1;
    }
    cookie_ = buf;
    std::string ip;
    int port;

    data_sock_ = std::make_shared<Udp>();
    result = data_sock_->Initialize();
    ASSERT(result >= 0);
    result = control_sock_->GetLocalAddress(ip, port);
    ASSERT(result >= 0);
    data_sock_->Bind(ip, port);

    buf = buf.substr(sizeof("cookie:") - 1);
    int index = buf.find(':');
    ip = buf.substr(0, index);
    port = atoi(buf.substr(index + 1).c_str());
    data_sock_->Connect(ip, port);

    LOGW("connect new client: %s:%d (%s)\n", ip.c_str(), port, cookie_.c_str());
    return 0;
}

int Peer::Timeout(int timeout)
{
    if(!commandsender_) return 0;
    return commandsender_->Timeout(timeout);
}

void Peer::SetCommand(std::shared_ptr<Command> command)
{
    std::shared_ptr<CommandChannel> channel(new CommandChannel{command, context_, control_sock_, data_sock_});
    commandsender_ = command->CreateCommandSender(channel);
    ASSERT_RETURN(commandsender_);
    context_->SetWriteFd(control_sock_->GetFd());
}
