
#include <unistd.h>
#include <sys/un.h>

#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <functional>
#include <thread>
#include <chrono>

#include "sock.h"
#include "command.h"
#include "context2.h"
#include "netsnoop.h"
#include "net_snoop_server.h"

using namespace std::chrono;

/**
 * @brief run main command loop.
 * 
 * @return int if success return 0 else return -1
 */
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
    ASSERT_RETURN(result == 0,-1,"create pipe failed.");
    context_->SetReadFd(pipefd_[0]);

    result = StartListen();
    ASSERT_RETURN(result >= 0,-1,"start server error.");

    high_resolution_clock::time_point start, end;
    int64_t time_spend;
    while (true)
    {
        if (timeout_ptr)
        {
            for (auto &peer : peers_)
            {
                end = high_resolution_clock::now();
                time_spend = duration_cast<milliseconds>(end - start).count();
                if (time_spend > 0 && peer->GetTimeout() > 0)
                    peer->Timeout(time_spend);
                //ASSERT(result>=0);
            }
        }

        result = ProcessNextCommand();
        ASSERT(result == 0);

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
                LOGVP("Set timeout: %ld", timeout.tv_sec * 1000 + timeout.tv_nsec / 1000 / 1000);
            }
        }

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
        LOGVP("selecting...");
        result = pselect(context_->max_fd + 1, &read_fdsets, &write_fdsets, NULL, timeout_ptr, NULL);
        LOGVP("selected---------------");
#ifdef _DEBUG
        for (int i = 0; i < context_->max_fd + 1; i++)
        {
            if (FD_ISSET(i, &read_fdsets))
            {
                LOGVP("can read: %d", i);
            }
            if (FD_ISSET(i, &write_fdsets))
            {
                LOGVP("can write: %d", i);
            }
        }
#endif
        if (result < 0)
        {
            // Todo: close socket
            LOGEP("select error: %s(errno: %d)", strerror(errno), errno);
            return -1;
        }

        if (result == 0)
        {
            LOGVP("time out: %d", time_millseconds);
            continue;
        }
        if (FD_ISSET(pipefd_[0], &read_fdsets))
        {
            result = AcceptNewCommand();
            ASSERT_RETURN(result >= 0, -1, "accept new command error.");
        }
        if (FD_ISSET(context_->control_fd, &read_fdsets))
        {
            result = AceeptNewConnect();
            ASSERT_RETURN(result >= 0, -1, "accept new connect error.");
        }
        // process clients
        for (auto it = peers_.begin(); it != peers_.end();)
        {
            result = 0;
            auto peer = *it;
            if (result >= 0 && FD_ISSET(peer->GetControlFd(), &write_fdsets))
            {
                result = peer->SendCommand();
            }
            if (result >= 0 && FD_ISSET(peer->GetControlFd(), &read_fdsets))
            {
                result = peer->RecvCommand();
            }
            if (peer->GetDataFd() > 0)
            {
                if (result >= 0 && FD_ISSET(peer->GetDataFd(), &write_fdsets))
                {
                    result = peer->SendData();
                }
                if (result >= 0 && FD_ISSET(peer->GetDataFd(), &read_fdsets))
                {
                    result = peer->RecvData();
                }
            }
            if (result < 0)
            {
                LOGWP("client removed: %s", peer->GetCookie().c_str());
                context_->ClrReadFd(peer->GetControlFd());
                context_->ClrWriteFd(peer->GetControlFd());
                if (peer->GetDataFd() > 0)
                {
                    context_->ClrReadFd(peer->GetDataFd());
                    context_->ClrWriteFd(peer->GetDataFd());
                }
                peer->Stop();
                it = peers_.erase(it);
                if(OnPeerDisconnected)
                    OnPeerDisconnected(peer.get());
            }
            else
                it++;
        }
    }

    return 0;
}

int NetSnoopServer::PushCommand(std::shared_ptr<Command> command)
{
    int result;
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_RETURN(command,-1);
    commands_.push(command);
    result = write(pipefd_[1],command->cmd.c_str(),command->cmd.length());
    ASSERT(result>0);
    return 0;
}

int NetSnoopServer::StartListen()
{
    int result;
    result = listen_peers_sock_->Initialize();
    ASSERT(result >= 0);
    result = listen_peers_sock_->Bind(option_->ip_local, option_->port);
    ASSERT(result >= 0);
    result = listen_peers_sock_->Listen(MAX_CLINETS);
    ASSERT(result >= 0);

    LOGIP("listen on %s:%d", option_->ip_local, option_->port);

    context_->control_fd = listen_peers_sock_->GetFd();
    context_->SetReadFd(listen_peers_sock_->GetFd());

    // result = command_sock_->Initialize();
    // result = command_sock_->Bind(option_->ip_local, option_->port);
    // result = command_sock_->Listen(MAX_SENDERS);
    // ASSERT(result >= 0);
    // context_->SetReadFd(command_sock_->GetFd());

    if (OnServerStart)
        OnServerStart(this);

    return 0;
}
int NetSnoopServer::AceeptNewConnect()
{
    int fd;

    if ((fd = listen_peers_sock_->Accept()) <= 0)
    {
        return -1;
    }

    auto tcp = std::make_shared<Tcp>(fd);
    auto peer = std::make_shared<Peer>(tcp, context_);
    peer->OnAuthSuccess = OnPeerConnected;
    peers_.push_back(peer);
    return 0;
}

int NetSnoopServer::AcceptNewCommand()
{
    int result;
    std::string cmd(MAX_CMD_LENGTH,0);
    result = read(pipefd_[0],&cmd[0],cmd.length());
    ASSERT_RETURN(result>0,-1);
    cmd.resize(result);
    return ProcessNextCommand();
}

int NetSnoopServer::ProcessNextCommand()
{
    if (is_running_ || commands_.empty())
    {
        return 0;
    }
    std::unique_lock<std::mutex> lock(mtx);
    // when commands is empty, commands_.front() causes random segmentfault.
    auto command = commands_.front();
    commands_.pop();
    lock.unlock();

    int result;
    auto ready_peers = &ready_peers_;
    for (auto &peer : peers_)
    {
        if (peer->IsReady())
            ready_peers->push_back(peer);
    }

    if (ready_peers->size() == 0)
    {
        ASSERT(command->cmd.length() > 3);
        command->InvokeCallback(NULL);
        while (commands_.size() > 0)
        {
            command = commands_.front();
            commands_.pop();
            command->InvokeCallback(NULL);
        }
        LOGDP("no client ready.");
        return 0;
    }

    LOGIP("start command: %s (peer count = %ld)", command->cmd.c_str(), ready_peers->size());
    auto peers_count = std::make_shared<int>(ready_peers->size());
    auto peers_failed = std::make_shared<int>(0);
    netstat_ = NULL;
    
    for (auto &peer : *ready_peers)
    {
        result = peer->SetCommand(command);
        ASSERT(result == 0);
        peer->OnStopped = [&, command, ready_peers, peers_count,peers_failed](const Peer *p, std::shared_ptr<NetStat> netstat) mutable {
            ready_peers->remove_if([&p](std::shared_ptr<Peer> p1) { return p1.get() == p; });
            LOGDP("stop command (%ld/%d): %s (%s)", (*peers_count - ready_peers->size()), *peers_count, command->cmd.c_str(), netstat ? netstat->ToString().c_str() : "NULL");
            if(OnPeerStopped) OnPeerStopped(p,netstat);
            if(netstat)
            {
                if(!netstat_)
                {
                    netstat_ = netstat;
                    netstat_->max_send_time = netstat->send_time;
                    netstat_->max_recv_time = netstat->recv_time;
                    netstat_->min_send_time = netstat->send_time;
                    netstat_->min_recv_time = netstat->recv_time;
                }
                else 
                {
                    *netstat_+=*netstat;
                    netstat_->max_send_time = std::max(netstat_->max_send_time,netstat->send_time);
                    netstat_->max_recv_time = std::max(netstat_->max_recv_time,netstat->recv_time);
                    netstat_->min_send_time = std::min(netstat_->min_send_time,netstat->send_time);
                    netstat_->min_recv_time = std::min(netstat_->min_recv_time,netstat->recv_time);
                }
            }
            else
            {
                (*peers_failed)++;
            }
            if (ready_peers->size() > 0)
            {
                return;
            }
            is_running_ = false;
            if(netstat_!=NULL)
            {
                auto success_count = *peers_count - *peers_failed;
                ASSERT(success_count>0);
                netstat_->loss /= success_count;
                netstat_->send_time /= success_count;
                netstat_->recv_time /= success_count;
                netstat_->delay /= success_count;
                netstat_->max_delay /= success_count;
                netstat_->min_delay /= success_count;
                netstat_->jitter /= success_count;
                netstat_->recv_avg_spped = netstat_->recv_speed/success_count;

                netstat_->peers_count = *peers_count;
                netstat_->peers_failed = *peers_failed;
            }
            LOGIP("command finish: %s || %s", command->cmd.c_str(), netstat_ ? netstat_->ToString().c_str() : "NULL");
            command->InvokeCallback(netstat_);
        };
        result = peer->Start();
        ASSERT(result == 0);
        is_running_ = true;
    }

    return 0;
}
