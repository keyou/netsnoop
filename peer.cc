
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
    // client closed or error.
    if(!commandsender_)
    {
        ASSERT_RETURN(control_sock_,-1);
        std::string buf(MAX_UDP_LENGTH,'\0');
        int result = control_sock_->Recv(&buf[0],buf.length());
        if(result<=0) 
        {
            LOGWP("recv command error(%d)",control_sock_->GetFd());
            return -1;
        }
        buf.resize(result);
        LOGWP("recv illegal command(%d): %s",control_sock_->GetFd(),Tools::GetDataSum(buf).c_str());
        return -1;
    }
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
    if(!commandsender_)
    {
        ASSERT_RETURN(data_sock_,-1);
        std::string buf(MAX_UDP_LENGTH,'\0');
        int result = data_sock_->Recv(&buf[0],buf.length());
        if(result<=0) 
        {
            LOGWP("recv data error(%d)",data_sock_->GetFd());
            return -1;
        }
        buf.resize(result);
        LOGWP("recv out of command data(%d): %s",data_sock_->GetFd(),Tools::GetDataSum(buf).c_str());
        return 0;
    }
    return commandsender_->RecvData();
}

int Peer::Auth()
{
    int result;
    std::string buf(MAX_UDP_LENGTH, '\0');
    if ((result = control_sock_->Recv(&buf[0], buf.length())) <= 0)
    {
        LOGEP("Disconnect.");
        return ERR_AUTH_ERROR;
    }
    buf.resize(result);

    if (buf.rfind("cookie:", 0) != 0)
    {
        LOGEP("Bad client.");
        return ERR_AUTH_ERROR;
    }
    cookie_ = buf;
    std::string local_ip;
    int local_port;
    result = control_sock_->GetLocalAddress(local_ip, local_port);
    ASSERT_RETURN(result >= 0,ERR_AUTH_ERROR);

    std::string remote_ip,peer_ip;
    int remote_port,peer_port;
    result = control_sock_->GetPeerAddress(remote_ip,remote_port);
    ASSERT_RETURN(result>=0,ERR_AUTH_ERROR);

    buf = buf.substr(sizeof("cookie:") - 1);
    int index = buf.find(':');
    peer_ip = buf.substr(0, index);
    peer_port = atoi(buf.substr(index + 1).c_str());
    // TODO: support NAT environment.
    ASSERT_RETURN(peer_ip==remote_ip,ERR_AUTH_ERROR,"support test on the same network only.");

    data_sock_ = std::make_shared<Udp>();
    result = data_sock_->Initialize();
    ASSERT_RETURN(result >= 0,ERR_AUTH_ERROR);
    result = data_sock_->Bind(local_ip, local_port);
    ASSERT_RETURN(result >= 0,ERR_AUTH_ERROR);
    result = data_sock_->Connect(peer_ip, peer_port);
    ASSERT_RETURN(result >= 0,ERR_AUTH_ERROR);
    context_->SetReadFd(data_sock_->GetFd());

    LOGDP("connect new client(fd=%d): %s:%d", data_sock_->GetFd(), peer_ip.c_str(), peer_port);
    if (OnAuthSuccess)
        OnAuthSuccess(this);

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
        auto interval = std::chrono::duration_cast<microseconds>(std::chrono::high_resolution_clock::now() - begin_).count();

        if (interval < command_->GetInterval())
        {
            return 0;
        }

        begin_ = std::chrono::high_resolution_clock::now();
        DataHead* head = (DataHead*)&buf[0];
        head->sequence = count_;
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
    current_sock_ = data_sock_;
    if (command->is_multicast)
    {
        ASSERT_RETURN(multicast_sock_,-1);
        MultiCastSock::Start();
        current_sock_ = std::make_shared<MultiCastSock>(multicast_sock_, data_sock_, command);
    }

    command_ = command;
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
