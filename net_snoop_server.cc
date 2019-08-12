
#include <unistd.h>
#include <sys/un.h>

#include <memory>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <functional>
#include <thread>
#include <chrono>

using namespace std::chrono;

#include "command.h"
#include "context2.h"
#include "netsnoop.h"
#include "net_snoop_server.h"

int NetSnoopServer::Run()
{
    int result;
    int time_millseconds = 0;
    timespec timeout = {0, 0};
    timespec *timeout_ptr = NULL;
    fd_set read_fdsets, write_fdsets;
    FD_ZERO(&read_fdsets);
    FD_ZERO(&write_fdsets);

    result = pipe(pipefd_);
    ASSERT(result == 0);

    result = StartListen();
    ASSERT(result >= 0);

    context_->SetReadFd(pipefd_[0]);
    LOGV("pipe fd: %d,%d\n", pipefd_[0], pipefd_[1]);

    std::vector<int> elements_to_remove;
    high_resolution_clock::time_point begin = high_resolution_clock::now();
    high_resolution_clock::time_point start, end;
    while (true)
    {
        if (timeout_ptr)
        {
            for (auto &peer : peers_)
            {
                end = high_resolution_clock::now();
                peer->Timeout(duration_cast<milliseconds>(end - start).count());
                //ASSERT(result>=0);
            }
        }
        memcpy(&read_fdsets, &context_->read_fds, sizeof(read_fdsets));
        memcpy(&write_fdsets, &context_->write_fds, sizeof(write_fdsets));
        timeout_ptr = NULL;
        time_millseconds = INT32_MAX;

        for (auto &peer : peers_)
        {
            if (peer->GetTimeout() > 0 && time_millseconds > peer->GetTimeout())
            {
                time_millseconds = peer->GetTimeout();
                timeout.tv_sec = time_millseconds / 1000;
                timeout.tv_nsec = (time_millseconds % 1000) * 1000 * 1000;
                timeout_ptr = &timeout;
                start = high_resolution_clock::now();
                LOGV("Set timeout: %ld\n", timeout.tv_sec * 1000 + timeout.tv_nsec / 1000 / 1000);
            }
        }

        result = AcceptNewCommand();
        ASSERT(result==0);

#ifdef _DEBUG
        for (int i = 0; i < sizeof(fd_set); i++)
        {
            if (FD_ISSET(i, &context_->read_fds))
            {
                LOGV("want read: %d\n", i);
            }
            if (FD_ISSET(i, &context_->write_fds))
            {
                LOGV("want write: %d\n", i);
            }
        }
#endif
        LOGV("selecting\n");
        result = pselect(context_->max_fd + 1, &read_fdsets, &write_fdsets, NULL, timeout_ptr, NULL);
        LOGV("selected---------------\n");
#ifdef _DEBUG
        for (int i = 0; i < sizeof(fd_set); i++)
        {
            if (FD_ISSET(i, &read_fdsets))
            {
                LOGV("can read: %d\n", i);
            }
            if (FD_ISSET(i, &write_fdsets))
            {
                LOGV("can write: %d\n", i);
            }
        }
#endif
        if (result < 0)
        {
            // Todo: close socket
            LOGE("select error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        if (result == 0)
        {
            LOGV("time out: %d\n", time_millseconds);
            continue;
        }
        if (FD_ISSET(pipefd_[0], &read_fdsets))
        {
            int result;
            std::string cmd(MAX_CMD_LENGTH, 0);
            result = read(pipefd_[0], &cmd[0], cmd.length());
            ASSERT(result > 0);
            cmd.resize(result);
            LOGV("Pipe read data: %s\n", cmd.c_str());
            result = AcceptNewCommand();
            ASSERT(result==0);
        }
        if (FD_ISSET(context_->control_fd, &read_fdsets))
        {
            result = AceeptNewPeer();
            ASSERT(result >= 0);
        }

        //for (auto &peer : peers_)
        for (auto it = peers_.begin(); it != peers_.end();)
        {
            auto peer = *it;
            LOGV("peer: cfd= %d, dfd= %d\n", peer->GetControlFd(), peer->GetDataFd());
            if (FD_ISSET(peer->GetControlFd(), &write_fdsets))
            {
                LOGV("Sending Command: cfd=%d\n", peer->GetControlFd());
                result = peer->SendCommand();
            }
            if (FD_ISSET(peer->GetControlFd(), &read_fdsets))
            {
                LOGV("Recving Command: cfd=%d\n", peer->GetControlFd());
                result = peer->RecvCommand();
            }
            if (peer->GetDataFd() > 0)
            {
                if (FD_ISSET(peer->GetDataFd(), &write_fdsets))
                {
                    LOGV("Sending Data: dfd=%d\n", peer->GetDataFd());
                    result = peer->SendData();
                }
                if (FD_ISSET(peer->GetDataFd(), &read_fdsets))
                {
                    LOGV("Recving Data: dfd=%d\n", peer->GetDataFd());
                    result = peer->RecvData();
                }
            }
            if (result < 0)
            {
                LOGE("client removed: %s\n", peer->GetCookie().c_str());
                context_->ClrReadFd(peer->GetControlFd());
                context_->ClrWriteFd(peer->GetControlFd());
                if (peer->GetDataFd() > 0)
                {
                    context_->ClrReadFd(peer->GetControlFd());
                    context_->ClrWriteFd(peer->GetControlFd());
                }
                if(peer->StopCallback) peer->StopCallback(peer.get(),NULL);
                it = peers_.erase(it);
            }
            else
                it++;
        }
    }

    return 0;
}
int NetSnoopServer::PushCommand(std::shared_ptr<Command> command)
{
    commands_.push(command);
    LOGV("Get cmd: %s\n", command->cmd.c_str());
    return write(pipefd_[1], command->cmd.c_str(), command->cmd.length());
}

int NetSnoopServer::StartListen()
{
    int result;
    result = listen_tcp_->Initialize();
    ASSERT(result >= 0);
    result = listen_tcp_->Bind(option_->ip_local, option_->port);
    ASSERT(result >= 0);
    result = listen_tcp_->Listen(MAX_CLINETS);
    ASSERT(result >= 0);

    LOGW("listen on %s:%d\n", option_->ip_local, option_->port);

    context_->control_fd = listen_tcp_->GetFd();
    context_->SetReadFd(listen_tcp_->GetFd());

    return 0;
}
int NetSnoopServer::AceeptNewPeer()
{
    int fd;

    if ((fd = listen_tcp_->Accept()) <= 0)
    {
        return -1;
    }

    auto tcp = std::make_shared<Tcp>(fd);
    auto peer = std::make_shared<Peer>(tcp, context_);
    peers_.push_back(peer);
    context_->SetReadFd(fd);

    std::string ip;
    int port;
    tcp->GetPeerAddress(ip, port);

    LOGW("connect new client: %s:%d\n", ip.c_str(), port);

    return fd;
}

int NetSnoopServer::AcceptNewCommand()
{
    auto command = commands_.front();
    if(is_running_ || !command) return 0;
    std::list<std::shared_ptr<Peer>> peers_copy = peers_;
    //auto copy_command = command->Clone();
    // TODO: deal with multi thread sync
    for (auto &peer : peers_)
    {
        if(peer->SetCommand(command)==0)
        {
            peer->StopCallback = [&,command,peers_copy](Peer* p,std::shared_ptr<NetStat> netstat) mutable{
                peers_copy.remove_if([&p](std::shared_ptr<Peer> p1){return p1.get() == p;});
                if(peers_copy.size()>0) return;
                is_running_ = false;
                //TODO: stat all netstat
                command->InvokeCallback(NULL);
            };
        }
    }
    is_running_ = true;
    commands_.pop();
    return 0; 
}
