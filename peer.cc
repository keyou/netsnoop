
#include <thread>

#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "command_sender.h"
#include "context2.h"
#include "peer.h"

Peer::Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Option> option, std::shared_ptr<Context> context)
    : control_sock_(control_sock), option_(option), context_(context)
{
    // keep control channel readable even no any data want to read,
    // because we use read to detect client disconnect.
    context_->SetReadFd(control_sock_->GetFd());
}

int Peer::Start()
{
    ASSERT_RETURN(commandsender_, -1);
    return commandsender_->Start();
}

int Peer::Stop()
{
    // clear peer status,can not stop command sender.
    // command sender can only stop by itself.
    if (OnStopped)
        OnStopped(this, NULL);
    commandsender_ = NULL;
    return 0; //commandsender_->Stop();
}

int Peer::SendCommand()
{
    ASSERT_RETURN(commandsender_, -1);
    return commandsender_->SendCommand();
}
int Peer::RecvCommand()
{
    if (cookie_.empty())
    {
        return Auth();
    }
    // client closed.
    if (!commandsender_)
        return -1;
    return commandsender_->RecvCommand();
}

int Peer::SendData()
{
    // TODO: optimize multicast logic
    if (!commandsender_)
        return 0;
    return commandsender_->SendData();
}

int Peer::RecvData()
{
    ASSERT_RETURN(commandsender_, -1);
    return commandsender_->RecvData();
}

int Peer::Auth()
{
    int result;
    std::string buf(1024, '\0');
    if ((result = control_sock_->Recv(&buf[0], buf.length())) <= 0)
    {
        LOGEP("Disconnect.");
        return -1;
    }
    buf.resize(result);

    if (buf.rfind("cookie:", 0) != 0)
    {
        LOGEP("Bad client.");
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

    // multicast_sock_ = std::make_shared<Udp>();
    // result = multicast_sock_->Initialize();
    // result = multicast_sock_->Bind(option_->ip_local,option_->port);
    // ASSERT_RETURN(result>=0,-1,"multicast socket bind error.");
    // //only recv the target's multicast packets
    // result = multicast_sock_->Connect(option_->ip_multicast, option_->port);
    // ASSERT_RETURN(result >= 0,-1,"multicast socket connect server error.");
    // LOGVP("multicast fd=%d",multicast_sock_->GetFd());

    buf = buf.substr(sizeof("cookie:") - 1);
    int index = buf.find(':');
    ip = buf.substr(0, index);
    port = atoi(buf.substr(index + 1).c_str());
    result = data_sock_->Connect(ip, port);
    ASSERT(result >= 0);

    if (OnAuthSuccess)
        OnAuthSuccess(this);
    LOGDP("connect new client: %s:%d (%s)", ip.c_str(), port, cookie_.c_str());
    return 0;
}

int Peer::Timeout(int timeout)
{
    if (!commandsender_)
        return 0;
    return commandsender_->Timeout(timeout);
}

class MultiCastSock : public Udp
{
public:
    MultiCastSock(std::shared_ptr<Sock> multicast_sock, std::shared_ptr<Sock> data_sock, std::shared_ptr<Command> command)
        : multicast_sock_(multicast_sock), data_sock_(data_sock), command_(std::dynamic_pointer_cast<SendCommand>(command))
    {
        fd_ = data_sock->GetFd();
    }
    ssize_t Send(const char *buf, size_t size) const override
    {
        //auto that = const_cast<MultiCastSock*>(this);
        if (count_ >= command_->GetCount())
        {
            return 0;
        }
        auto interval = std::chrono::duration_cast<milliseconds>(std::chrono::high_resolution_clock::now() - begin_).count();

        if (interval < command_->GetInterval())
        {
            return 0;
        }

        begin_ = std::chrono::high_resolution_clock::now();
        count_++;
        command_->is_finished = count_ >= command_->GetCount();
        return multicast_sock_->Send(buf, size);
    }
    static void Start()
    {
        begin_ = std::chrono::high_resolution_clock::time_point();
        count_ = 0;
    }

    ~MultiCastSock() override
    {
        fd_ = -1;
    }

private:
    static std::chrono::high_resolution_clock::time_point begin_;
    static int count_;
    std::shared_ptr<Sock> multicast_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::shared_ptr<SendCommand> command_;
};
int MultiCastSock::count_ = 0;
std::chrono::high_resolution_clock::time_point MultiCastSock::begin_;

int Peer::GetDataFd() const
{
    return data_sock_ ? data_sock_->GetFd() : -1;
}

int Peer::SetCommand(std::shared_ptr<Command> command)
{
    ASSERT_RETURN(data_sock_, -1);
    command_ = command;
    current_sock_ = data_sock_;
    if (command->is_multicast)
    {
        MultiCastSock::Start();
        current_sock_ = std::make_shared<MultiCastSock>(multicast_sock_, data_sock_, command_);
    }
    std::shared_ptr<CommandChannel> channel(new CommandChannel{
        command, context_, control_sock_, current_sock_});
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
