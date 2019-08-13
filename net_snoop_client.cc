

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

        LOGV("client[%d] selecting\n",context->control_fd);
        result = select(context->max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        LOGV("client[%d] selected\n",context->control_fd);
        ASSERT(result>0);
        if (result <= 0)
        {
            LOGE("select error: %d,%d\n", result, errno);
            return -1;
        }
        if (FD_ISSET(context->control_fd, &read_fds))
        {
            if ((result = RecvCommand()) < 0)
            {
                LOGE("client recv cmd error.\n");
                break;
            }
        }
        if (FD_ISSET(context->control_fd, &write_fds))
        {
            if ((result = SendCommand()) < 0)
            {
                LOGE("client send cmd error.\n");
                break;
            }
        }
        if (FD_ISSET(context->data_fd, &read_fds))
        {
            ASSERT(receiver_->Recv()>=0);
        }
        if (FD_ISSET(context->data_fd, &write_fds))
        {
            ASSERT(receiver_->Send()>=0);
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
    ASSERT(result >= 0);

    data_sock_ = std::make_shared<Udp>();
    data_sock_->Initialize();
    data_sock_->Connect(option_->ip_remote, option_->port);

    std::string ip;
    int port;
    result = data_sock_->GetLocalAddress(ip, port);
    ASSERT(result >= 0);

    std::string cookie("cookie:" + ip + ":" + std::to_string(port));
    result = control_sock_->Send(cookie.c_str(), cookie.length());
    ASSERT(result >= 0);

    context_->control_fd = control_sock_->GetFd();
    context_->data_fd = data_sock_->GetFd();

    return 0;
}

int NetSnoopClient::RecvCommand()
{
    int result;
    std::string cmd(64, '\0');
    if ((result = control_sock_->Recv(&cmd[0], cmd.length())) <= 0)
    {
        return -1;
    }
    cmd.resize(result);

    auto command = CommandFactory::New(cmd);
    if (!command)
        return ERR_ILLEGAL_DATA;
    if(receiver_ && command->is_private)
    {
        return receiver_->RecvPrivateCommand(command);
    }
    if(receiver_) receiver_->Stop();
    auto channel = std::shared_ptr<CommandChannel>(new CommandChannel{
        command,context_,control_sock_,data_sock_
    });
    receiver_ = command->CreateCommandReceiver(channel);
    ASSERT(receiver_);
    return receiver_->Start();
}

int NetSnoopClient::SendCommand()
{
    ASSERT(receiver_);
    return receiver_->SendPrivateCommand();
}
