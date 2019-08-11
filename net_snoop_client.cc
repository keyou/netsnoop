

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

        LOGV("selecting\n");
        result = select(context->max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        LOGV("selected\n");

        if (result <= 0)
        {
            LOGE("select error: %d,%d\n", result, errno);
            return -1;
        }
        if (FD_ISSET(context->control_fd, &read_fds))
        {
            if ((result = ReceiveCommand()) < 0)
            {
                LOGE("Parsing cmd error.\n");
                break;
            }
        }
        if (FD_ISSET(context->control_fd, &write_fds))
        {
        }
        if (FD_ISSET(context->data_fd, &read_fds))
        {
            receiver_->Recv();
        }
        if (FD_ISSET(context->data_fd, &write_fds))
        {
            receiver_->Send();
        }
    }

    return -1;
}

int NetSnoopClient::Connect()
{
    int result;
    tcp_client_ = std::make_shared<Tcp>();
    result = tcp_client_->Initialize();
    ASSERT(result > 0);
    result = tcp_client_->Connect(option_->ip_remote, option_->port);
    ASSERT(result >= 0);

    // char buf[1024 * 64] = {0};
    // ssize_t rlength = 0;
    // srand(high_resolution_clock::now().time_since_epoch().count());
    // std::string cookie = "cookie:" + std::to_string(rand());

    // result = tcp_client_->Send(cookie.c_str(), cookie.length());
    // ASSERT(result >= 0);

    // result = tcp_client_->Recv(buf, sizeof(buf));
    // ASSERT(result >= 0);
    // ASSERT(cookie == buf);

    udp_client_ = std::make_shared<Udp>();
    udp_client_->Initialize();
    udp_client_->Connect(option_->ip_remote, option_->port);

    std::string ip;
    int port;
    result = udp_client_->GetLocalAddress(ip, port);
    ASSERT(result >= 0);

    std::string cookie("cookie:" + ip + ":" + std::to_string(port));
    result = tcp_client_->Send(cookie.c_str(), cookie.length());
    ASSERT(result >= 0);

    context_->control_fd = tcp_client_->GetFd();
    context_->data_fd = udp_client_->GetFd();

    return 0;
}

int NetSnoopClient::ReceiveCommand()
{
    int result;
    std::string cmd(64, '\0');
    LOGV("Client parsing cmd.\n");
    if ((result = tcp_client_->Recv(&cmd[0], cmd.length())) <= 0)
    {
        return -1;
    }
    cmd.resize(result);

    auto command = CommandFactory::New(cmd);
    if (!command)
        return ERR_ILLEGAL_DATA;
    if(receiver_) receiver_->Stop();
    if (command->name == "echo") receiver_ = std::make_shared<EchoCommandReceiver>(context_);
    if (command->name == "recv") receiver_ = std::make_shared<RecvCommandReceiver>(context_);
    ASSERT(receiver_);
    return receiver_->Start();
}
