

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
    context->SetReadFd(context->control_fd);

    while (true)
    {
        memcpy(&read_fds, &context->read_fds, sizeof(read_fds));
        memcpy(&write_fds, &context->write_fds, sizeof(write_fds));

#ifdef _DEBUG
        for (int i = 0; i < context_->max_fd + 1; i++)
        {
            if (FD_ISSET(i, &context_->read_fds))
            {
                LOGVP("want read: %d", i);
            }
            if (FD_ISSET(i, &context_->write_fds))
            {
                LOGVP("want write: %d", i);
            }
        }
#endif

        LOGVP("client[%d] selecting",context->control_fd);
        result = select(context->max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        LOGVP("client[%d] selected",context->control_fd);
        ASSERT(result>0);
        if (result <= 0)
        {
            LOGEP("select error: %d,%d", result, errno);
            return -1;
        }
        if (FD_ISSET(context->data_fd, &read_fds))
        {
            result = receiver_->Recv();
            ASSERT(result>=0);
        }
        if (FD_ISSET(context->data_fd, &write_fds))
        {
            result = receiver_->Send();
            ASSERT(result>=0);
        }
        if (FD_ISSET(context->control_fd, &write_fds))
        {
            if ((result = SendCommand()) < 0)
            {
                LOGEP("client send cmd error.");
                break;
            }
        }
        if (FD_ISSET(context->control_fd, &read_fds))
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
    control_sock_ = std::make_shared<Tcp>();
    result = control_sock_->Initialize();
    ASSERT(result > 0);
    result = control_sock_->Connect(option_->ip_remote, option_->port);
    ASSERT_RETURN(result >= 0,-1,"connect server error.");

    data_sock_ = std::make_shared<Udp>();
    result = data_sock_->Initialize();
    result = data_sock_->Connect(option_->ip_remote, option_->port);
    ASSERT_RETURN(result >= 0,-1,"data socket connect server error.");

    multicast_sock_ = std::make_shared<Udp>();
    result = multicast_sock_->Initialize();
    result = multicast_sock_->Bind(option_->ip_multicast,option_->port);
    ASSERT_RETURN(result >= 0,-1,"multicast socket bind error: %s:%d",option_->ip_multicast,option_->port);
    //only recv the target's multicast packets
    result = multicast_sock_->Connect(option_->ip_remote, option_->port);
    ASSERT_RETURN(result >= 0,-1,"multicast socket connect server error.");

    std::string ip_local,ip_remote;
    int port_local,port_remote;

    result = multicast_sock_->GetLocalAddress(ip_local,port_local);
    ASSERT(result>=0);
    result = multicast_sock_->GetPeerAddress(ip_remote,port_remote);
    ASSERT(result>=0);
    LOGDP("multicast connection: %s:%d -> %s:%d",ip_local.c_str(),port_local,ip_remote.c_str(),port_remote);
    // while(true){
    // std::string data(100,0);
    // result = multicast_sock_->Recv(&data[0],100);
    // }

    result = data_sock_->GetLocalAddress(ip_local, port_local);
    ASSERT(result >= 0);
    std::string cookie("cookie:" + ip_local + ":" + std::to_string(port_local));
    result = control_sock_->Send(cookie.c_str(), cookie.length());
    ASSERT_RETURN(result >= 0,-1);

    context_->control_fd = control_sock_->GetFd();
    context_->data_fd = data_sock_->GetFd();

    return 0;
}

int NetSnoopClient::RecvCommand()
{
    int result;
    std::string cmd(64, '\0');
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
    LOGDP("recv new command: %s",command->cmd.c_str());
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
    std::string buf(MAX_UDP_LENGTH,0);
    while ((result = recv(data_sock_->GetFd(),&buf[0],buf.length(),MSG_DONTWAIT))>0)
    {
        LOGDP("illegal data recved(%d).",result);
    }
#endif // !WIN32

    auto ack_command = std::make_shared<AckCommand>();
    result = control_sock_->Send(ack_command->cmd.c_str(),ack_command->cmd.length());
    ASSERT_RETURN(result>0,ERR_DEFAULT,"send ack command error.");

    context_->data_fd = command->is_multicast?multicast_sock_->GetFd():data_sock_->GetFd();
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
    return receiver_->SendPrivateCommand();
}
