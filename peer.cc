
#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "peer.h"
#include "context2.h"

int Peer::SendCommand()
{
    if (command_->id == CMD_ECHO)
    {
        if (control_sock_->Send(command_->cmd.c_str(), command_->cmd.length()) == -1)
        {
            LOGE("change mode error.\n");
            return -1;
        }
        context_->SetWriteFd(data_sock_->GetFd());
        context_->ClrReadFd(data_sock_->GetFd());
        command_->id = CMD_ECHO;
        count_ = 0;
        timeout_ = ((EchoCommand *)command_.get())->GetInterval();
        timeout_callback_ = [&]() {
            if (count_ >= ((EchoCommand *)command_.get())->GetCount())
                return 0;
            context_->SetWriteFd(data_sock_->GetFd());
            timeout_ = ((EchoCommand *)command_.get())->GetInterval();
            return 0;
        };
    }
    // Stop send control cmd and Start recv control cmd.
    context_->ClrWriteFd(control_sock_->GetFd());
    context_->SetReadFd(control_sock_->GetFd());
    return 0;
}
int Peer::RecvCommand()
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
        // auto data = context_->peers.erase(std::remove(context_->peers.begin(),context_->peers.end(), std::make_shared<Peer>(this)),context_->peers.end());
        // ASSERT(data!=context_->peers.end());
        return -1;
    }
    buf.resize(result);
    if (cookie_.empty())
    {
        if (buf.rfind("cookie:", 0) != 0)
        {
            LOGE("Bad client.\n");
            // auto data = context_->peers.erase(std::remove(context_->peers.begin(),context_->peers.end(),std::make_shared<Peer>(this)),context_->peers.end());
            // ASSERT(data!=context_->peers.end());
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

    LOGV("Recv Action: %s\n", buf.c_str());

    return 0;
}

int Peer::SendData()
{
    if (command_->id == CMD_ECHO)
        return SendEcho();
    if (command_->id == CMD_RECV)
        return SendEcho();
    if (command_->id == CMD_SEND)
        return SendEcho();
    LOGE("Peer send error: cmd = %d\n", command_->id);
#define ERR_OTHER -99
    return ERR_OTHER;
}
int Peer::RecvData()
{
    if (command_->id == CMD_ECHO)
        return RecvEcho();
    if (command_->id == CMD_RECV)
        return RecvEcho();
    if (command_->id == CMD_SEND)
        return RecvEcho();
    LOGE("Peer recv error: cmd = %d\n", command_->id);
#define ERR_OTHER -99
    return ERR_OTHER;
}

int Peer::SendEcho()
{
    count_++;
    context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    static unsigned long i = 0;
    const std::string tmp(std::to_string(++i));
    return data_sock_->Send(tmp.c_str(), tmp.length());
}

int Peer::RecvEcho()
{
    //context_->SetWriteFd(data_fd_);
    context_->ClrReadFd(data_sock_->GetFd());
    return data_sock_->Recv(buf_, sizeof(buf_));
}

int Peer::Timeout(int timeout)
{
    if (!timeout_callback_ || timeout_ <= 0)
        return 0;
    timeout_ -= timeout;
    if (timeout_ <= 0)
        return timeout_callback_();
    return 0;
}

void Peer::SetCommand(std::shared_ptr<Command> command)
{
    command_ = command;
    context_->SetWriteFd(control_sock_->GetFd());
}
