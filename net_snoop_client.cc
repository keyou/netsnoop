

#include "context2.h"
#include "command.h"
#include "net_snoop_client.h"

int NetSnoopClient::Run()
{
    int result;
    char buf[100] = {0};
    fd_set read_fds, write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    context_ = std::make_shared<Context>();
    auto context = context_;

    if ((result = Connect()) != 0)
        return result;

    while (true)
    {
        memcpy(&read_fds, &context->read_fds, sizeof(read_fds));
        memcpy(&write_fds, &context->write_fds, sizeof(write_fds));

#ifdef _DEBUG
        for (int i = 0; i < context_->max_fd + 1; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                LOGVP("want read: %d", i);
            }
            if (FD_ISSET(i, &write_fds))
            {
                LOGVP("want write: %d", i);
            }
        }
#endif
        LOGVP("client[%d] selecting",control_sock_->GetFd());
        result = select(context->max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        LOGVP("client[%d] selected",control_sock_->GetFd());
#ifdef _DEBUG
        for (int i = 0; i < context_->max_fd + 1; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                LOGVP("can read: %d", i);
            }
            if (FD_ISSET(i, &write_fds))
            {
                LOGVP("can write: %d", i);
            }
        }
#endif
        if (result <= 0)
        {
            PSOCKETERROR("select error");
            return -1;
        }
        if (FD_ISSET(data_sock_->GetFd(), &write_fds))
        {
            result = SendData();
            ASSERT_RETURN(result>=0,-1);
        }
        if (FD_ISSET(data_sock_->GetFd(), &read_fds))
        {
            result = RecvData(data_sock_);
            if(result<=0) LOGWP("data sock recv error.");
            // in a long delay network(>100ms)，we may recv a port unreachable ICMP packet.
            //ASSERT_RETURN(result>0,-1);
        }
        if (FD_ISSET(multicast_sock_->GetFd(), &read_fds))
        {
            result = RecvData(multicast_sock_);
            ASSERT_RETURN(result>0,-1);
        }
        if (FD_ISSET(control_sock_->GetFd(), &write_fds))
        {
            if ((result = SendCommand()) < 0)
            {
                LOGEP("client send cmd error.");
                break;
            }
        }
        if (FD_ISSET(control_sock_->GetFd(), &read_fds))
        {
            if ((result = RecvCommand()) == ERR_DEFAULT)
            {
                LOGEP("client recv cmd error.");
                break;
            }
            else if(result == ERR_SOCKET_CLOSED)
            {
                LOGWP("client control socket closed.");
                break;
            }
        }
    }

    return -1;
}

int NetSnoopClient::Connect()
{
    int result;
    int retry = 3;
    control_sock_ = std::make_shared<Tcp>();
    result = control_sock_->Initialize();
    ASSERT(result > 0);

    while(retry>0)
    {
        result = control_sock_->Connect(option_->ip_remote, option_->port);
        if(result>=0) break;
        LOGWP("connect server error,retrying...");
        sleep(1);
        retry--;
        if(retry == 0)
        {
            LOGEP("connect to %s:%d error.",option_->ip_remote,option_->port);
            return -1;
        }
    }
    
    data_sock_ = std::make_shared<Udp>();
    result = data_sock_->Initialize();
    result = data_sock_->Connect(option_->ip_remote, option_->port);
    ASSERT_RETURN(result >= 0,-1,"data socket connect server error.");

    std::string ip_local;
    int port_local;
    result = data_sock_->GetLocalAddress(ip_local, port_local);
    ASSERT(result >= 0);

    multicast_sock_ = std::make_shared<Udp>();
    result = multicast_sock_->Initialize();
    result = multicast_sock_->Bind("0.0.0.0",option_->port);
    ASSERT_RETURN(result >= 0,-1,"multicast socket bind error: %s:%d",ip_local.c_str(),option_->port);
    result = multicast_sock_->JoinMUlticastGroup(option_->ip_multicast,ip_local);
    ASSERT_RETURN(result>=0,-1);

    // only recv the target's multicast packets,windows can not do this
    // result = multicast_sock_->Connect(option_->ip_remote, option_->port);
    // ASSERT_RETURN(result >= 0,-1,"multicast socket connect server error.");

    std::string cookie("cookie:" + ip_local + ":" + std::to_string(port_local));
    result = control_sock_->Send(cookie.c_str(), cookie.length());
    ASSERT_RETURN(result >= 0,-1);
    // TODO: optimize this code, wait 100 millseconds for server creating the data sock
    usleep(500*1000);
    // to make a hole in the firewall.
    result = data_sock_->Send(cookie.c_str(), cookie.length());
    ASSERT_RETURN(result >= 0,-1);

    context_->control_fd = control_sock_->GetFd();
    context_->data_fd = data_sock_->GetFd();

    context_->SetReadFd(control_sock_->GetFd());
    context_->SetReadFd(data_sock_->GetFd());
    context_->SetReadFd(multicast_sock_->GetFd());

    if(OnConnected) OnConnected();

    return 0;
}

int NetSnoopClient::RecvCommand()
{
    int result;
    std::string cmd(MAX_UDP_LENGTH, '\0');
    if ((result = control_sock_->Recv(&cmd[0], cmd.length())) < 0)
    {
        return ERR_DEFAULT;
    }
    if(result == 0)
    {
        // socket closed.
        return ERR_SOCKET_CLOSED;
    }
    cmd.resize(result);

    auto command = CommandFactory::New(cmd);
    if (!command)
        return ERR_ILLEGAL_DATA;
    LOGDP("recv new command: %s",command->GetCmd().c_str());
    if(receiver_ && command->is_private)
    {
        auto stop_command = std::dynamic_pointer_cast<StopCommand>(command);
        if(stop_command)
        {
            return receiver_->Stop();    
        }
        return receiver_->RecvPrivateCommand(command);
    }
    
#ifndef WIN32
    // clear data socket data.
    // std::string buf(MAX_UDP_LENGTH,0);
    // while ((result = recv(data_sock_->GetFd(),&buf[0],buf.length(),MSG_DONTWAIT))>0)
    // {
    //     LOGDP("illegal data recved(%d).",result);
    // }
#endif // !WIN32

    auto ack_command = std::make_shared<AckCommand>();
    result = control_sock_->Send(ack_command->GetCmd().c_str(),ack_command->GetCmd().length());
    ASSERT_RETURN(result>0,ERR_DEFAULT,"send ack command error.");

    auto channel = std::shared_ptr<CommandChannel>(new CommandChannel{
        command,context_,control_sock_,command->is_multicast?multicast_sock_:data_sock_
    });
    receiver_ = command->CreateCommandReceiver(channel);
    ASSERT(receiver_);
    receiver_->OnStopped = OnStopped;
    return receiver_->Start();
}

int NetSnoopClient::SendCommand()
{
    ASSERT(receiver_);
    receiver_->out_of_command_packets_ = illegal_packets_;
    int result = receiver_->SendPrivateCommand();
    receiver_ = NULL;
    illegal_packets_ = 0;
    return result;
}

int NetSnoopClient::RecvData(std::shared_ptr<Sock> data_sock)
{
    int result;
    if(!receiver_)
    {
        std::string buf(MAX_UDP_LENGTH,'\0');
        result = data_sock->Recv(&buf[0],buf.length());
        if(result<=0)
        {
            LOGWP("recv data error(%d).",data_sock->GetFd());
            return result;
        }
        buf.resize(result);
        illegal_packets_++;
        LOGWP("recv out of command data(%d): %s",data_sock->GetFd(),Tools::GetDataSum(buf).c_str());
        return result;
    }
    result = receiver_->Recv();
    return result;
}

int NetSnoopClient::SendData()
{
    if(!receiver_)
    {
        context_->ClrWriteFd(data_sock_->GetFd());
        LOGWP("send out of command data(%d).",data_sock_->GetFd());
        return 0;
    }
    int result = receiver_->Send();
    return result;
}