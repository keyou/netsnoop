
#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "command_sender.h"
#include "context2.h"
#include "peer.h"

Peer::Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context)
    : Peer(control_sock, "", context)
{
}

Peer::Peer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context)
    : cookie_(cookie), context_(context), control_sock_(control_sock)
{
    // keep control channel readable even no any data want to read,
    // because we use read to detect client disconnect.
    context_->SetReadFd(control_sock_->GetFd());
}

int Peer::Start()
{
    ASSERT_RETURN(commandsender_,-1);
    return commandsender_->Start();
}

int Peer::Stop()
{
    // clear peer status,can not stop command sender.
    // command sender can only stop by itself.
    if (OnStopped)
        OnStopped(this, NULL);
    commandsender_ = NULL;
    return 0;//commandsender_->Stop();
}

int Peer::SendCommand()
{
    ASSERT_RETURN(commandsender_,-1);
    return commandsender_->SendCommand();
}
int Peer::RecvCommand()
{
    if (cookie_.empty())
    {
        return Auth();
    }
    // client closed.
    if(!commandsender_) return -1;
    return commandsender_->RecvCommand();
}

int Peer::SendData()
{
    ASSERT_RETURN(commandsender_,-1);
    return commandsender_->SendData();
}

int Peer::RecvData()
{
    ASSERT_RETURN(commandsender_,-1);
    return commandsender_->RecvData();
}

int Peer::Auth()
{
    int result;
    std::string buf(1024, '\0');
    if ((result = control_sock_->Recv(&buf[0], buf.length())) <= 0)
    {
        LOGE("Disconnect.");
        return -1;
    }
    buf.resize(result);

    if (buf.rfind("cookie:", 0) != 0)
    {
        LOGE("Bad client.");
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
    result = data_sock_->Bind(ip, port);
    ASSERT(result >= 0);

    buf = buf.substr(sizeof("cookie:") - 1);
    int index = buf.find(':');
    ip = buf.substr(0, index);
    port = atoi(buf.substr(index + 1).c_str());
    result = data_sock_->Connect(ip, port);
    ASSERT(result >= 0);

    if (OnAuthSuccess)
        OnAuthSuccess(this);
    LOGW("connect new client: %s:%d (%s)", ip.c_str(), port, cookie_.c_str());
    return 0;
}

int Peer::Timeout(int timeout)
{
    if (!commandsender_)
        return 0;
    return commandsender_->Timeout(timeout);
}

int Peer::SetCommand(std::shared_ptr<Command> command)
{
    ASSERT_RETURN(data_sock_,-1);
    std::shared_ptr<CommandChannel> channel(new CommandChannel{command, context_, control_sock_, data_sock_});
    commandsender_ = command->CreateCommandSender(channel);
    ASSERT_RETURN(commandsender_, -1);
    commandsender_->OnStopped = [&](std::shared_ptr<NetStat> netstat) {
        if (OnStopped)
            OnStopped(this, netstat);
        //commandsender_ is not reusable.
        commandsender_ = NULL;
    };
    return 0;
}
