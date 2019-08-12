
#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "context2.h"
#include "peer.h"
#include "command_sender.h"

CommandSender::CommandSender(std::shared_ptr<CommandChannel> channel)
    : timeout_(-1), control_sock_(channel->control_sock_), data_sock_(channel->data_sock_),
      context_(channel->context_), command_(channel->command_)
{
}

int CommandSender::SendCommand()
{
    int result;
    if ((result = control_sock_->Send(command_->cmd.c_str(), command_->cmd.length())) == -1)
    {
        LOGE("change commandsender error.\n");
        return -1;
    }
    return result;
}

int CommandSender::Timeout(int timeout)
{
    if (timeout_ <= 0)
        return 0;
    timeout_ -= timeout;
    if (timeout_ <= 0)
        return OnTimeout();
    return 0;
}

    EchoCommandSender::EchoCommandSender(std::shared_ptr<CommandChannel> channel)
        : command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)), buf_{0}, count_(0),CommandSender(channel)
    {
    }

int EchoCommandSender::SendCommand()
{
    start_ = high_resolution_clock::now();
    CommandSender::SendCommand();
    context_->SetWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    SetTimeout(((EchoCommand *)command_.get())->GetInterval());
    count_ = 0;
    return 0;
}

int EchoCommandSender::RecvCommand()
{
    int result;
    char buf[64] = {0};
    result = control_sock_->Recv(buf, sizeof(buf));
    ASSERT_RETURN(result>0,-1);
    return 0;
}

int EchoCommandSender::SendData()
{
    count_++;
    context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    static unsigned long i = 0;
    const std::string tmp(std::to_string(++i));
    return data_sock_->Send(tmp.c_str(), tmp.length());
}

int EchoCommandSender::RecvData()
{
    //context_->SetWriteFd(data_fd_);
    context_->ClrReadFd(data_sock_->GetFd());
    return data_sock_->Recv(buf_, sizeof(buf_));
}

int EchoCommandSender::OnTimeout()
{
    if (count_ >= ((EchoCommand *)command_.get())->GetCount())
    {
        stop_ = high_resolution_clock::now();
        command_->InvokeCallback(NULL);
        return 0;
    }
    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(((EchoCommand *)command_.get())->GetInterval());
    return 0;
}

using RecvCommandClazz = class RecvCommand;
RecvCommandSender::RecvCommandSender(std::shared_ptr<CommandChannel> channel)
        : command_(std::dynamic_pointer_cast<RecvCommandClazz>(channel->command_)),CommandSender(channel)
    {
    }

int RecvCommandSender::SendCommand()
{
    int result = CommandSender::SendCommand();
    ASSERT_RETURN(result>0,-1);

    context_->SetWriteFd(data_sock_->GetFd());
    buf_ = std::string(10, 'x');
    count_ = 0;
    SetTimeout(100);
    return 0;
}
int RecvCommandSender::RecvCommand()
{
    int result;
    char buf[64] = {0};
    result = control_sock_->Recv(buf, sizeof(buf));
    ASSERT_RETURN(result>0,-1);
    //TODO: deal with the recv command
    return 0;
}
int RecvCommandSender::SendData()
{
    count_++;
    context_->ClrWriteFd(data_sock_->GetFd());
    return data_sock_->Send(buf_.c_str(), buf_.length());
}
int RecvCommandSender::RecvData()
{
    return 0;
}
int RecvCommandSender::OnTimeout()
{
    if (count_ >= 10)
    {
        context_->ClrWriteFd(data_sock_->GetFd());
        command_->InvokeCallback(NULL);
    }
    else
    {
        context_->SetWriteFd(data_sock_->GetFd());
        SetTimeout(100);
    }

    return 0;
}