
#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "peer.h"
#include "context2.h"

int Peer::SendCommand()
{
    ASSERT(mode_);
    mode_->SendCommand();
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
    if(!mode_) 
    {
        LOGE("illegal data.\n");
        return -1;
    }

    return mode_->RecvCommand();
}

int Peer::SendData()
{
    ASSERT(mode_);
    return mode_->SendData();
}

int Peer::RecvData()
{
    ASSERT(mode_);
    return mode_->RecvData();
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
        timeout_ = 0;
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
    if (timeout_ <= 0)
        return 0;
    timeout_ -= timeout;
    if (timeout_ <= 0)
        return mode_->OnTimeout();
    return 0;
}

void Peer::SetCommand(std::shared_ptr<Command> command)
{
    if (command->id == CMD_ECHO)
        mode_ = std::make_shared<EchoMode>(this, command);
    if(command->id == CMD_RECV)
        mode_ = std::make_shared<RecvMode>(this, command);
    ASSERT(mode_);
    context_->SetWriteFd(control_sock_->GetFd());
}
int Mode::SendCommand()
{
    if (control_sock_->Send(command_->cmd.c_str(), command_->cmd.length()) == -1)
    {
        LOGE("change mode error.\n");
        return -1;
    }
    return 0;
}
int EchoMode::SendCommand()
{
    start_ = high_resolution_clock::now();
    Mode::SendCommand();
    context_->SetWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    SetTimeout(((EchoCommand *)command_.get())->GetInterval());
    count_ = 0;
    return 0;
}

int EchoMode::RecvCommand()
{
    int result;
    char buf[64] = {0};
    result = control_sock_->Recv(buf,sizeof(buf));
    if(result <= 0) return -1;
    //TODO: deal with the recv command
    return 0;
}

int EchoMode::SendData()
{
    count_++;
    context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    static unsigned long i = 0;
    const std::string tmp(std::to_string(++i));
    return data_sock_->Send(tmp.c_str(), tmp.length());
}

int EchoMode::RecvData()
{
    //context_->SetWriteFd(data_fd_);
    context_->ClrReadFd(data_sock_->GetFd());
    return data_sock_->Recv(buf_, sizeof(buf_));
}

int EchoMode::OnTimeout()
{
    if (count_ >= ((EchoCommand *)command_.get())->GetCount())
    {
        stop_ = high_resolution_clock::now();
        return 0;
    }
    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(((EchoCommand *)command_.get())->GetInterval());
    return 0;
}

int RecvMode::SendCommand()
{
    int result = Mode::SendCommand();
    RETURN_IF_NEG(result);

    context_->SetWriteFd(data_sock_->GetFd());
    buf_ = std::string(10,'x');
    count_=0;
    SetTimeout(100);
    return 0;
}
int RecvMode::RecvCommand()
{
    return 0;
}
int RecvMode::SendData()
{
    count_++;
    context_->ClrWriteFd(data_sock_->GetFd());
    return data_sock_->Send(buf_.c_str(),buf_.length());
}
int RecvMode::RecvData()
{
    return 0;
}
int RecvMode::OnTimeout()
{
    if(count_>=10) 
    {
        context_->ClrWriteFd(data_sock_->GetFd());
    }
    else
    {
        context_->SetWriteFd(data_sock_->GetFd());
        SetTimeout(100);
    }
    
    return 0;
}