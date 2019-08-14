
#include <cmath>
#include <functional>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "context2.h"
#include "peer.h"
#include "command_sender.h"

CommandSender::CommandSender(std::shared_ptr<CommandChannel> channel)
    : timeout_(-1), control_sock_(channel->control_sock_), data_sock_(channel->data_sock_),
      context_(channel->context_), command_(channel->command_),
      is_stopping_(false), is_stopped_(false), is_stop_command_send_(false),
      is_starting_(false), is_started_(false)
{
}

int CommandSender::Start()
{
    is_starting_ = true;
    // allow control channel write fist command
    context_->SetWriteFd(control_sock_->GetFd());
    // allow control channel read command even no any command want to read,
    // because we use read to detect client disconnected.
    context_->SetReadFd(control_sock_->GetFd());
    return OnStart();
}
int CommandSender::OnStart()
{
    return 0;
}
/**
 * @brief Stop data channel and start stop
 * 
 * @return int 
 */
int CommandSender::Stop()
{
    is_stopping_ = true;
    // stop data channel
    context_->ClrWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    // allow to send stop command
    context_->SetWriteFd(control_sock_->GetFd());
    return 0;
}

int CommandSender::OnStop(std::shared_ptr<NetStat> netstat)
{
    if(OnStopped) OnStopped(netstat);
    return 0;
}

int CommandSender::SendCommand()
{
    int result;
    // stop control channel write
    context_->ClrWriteFd(control_sock_->GetFd());

    if (is_stopping_)
    {
        ASSERT(is_started_);
        is_stop_command_send_ = true;
        LOGV("CommandSender send stop command. (%s)\n", command_->cmd.c_str());
        auto stop_command = std::make_shared<StopCommand>();
        result = control_sock_->Send(stop_command->cmd.c_str(), stop_command->cmd.length());
        if(result <= 0) return -1;
        return result;
    }
    if (is_starting_)
    {
        is_starting_ = false;
        is_started_ = true;
    }
    return OnSendCommand();
}

int CommandSender::OnSendCommand()
{
    int result;
    if ((result = control_sock_->Send(command_->cmd.c_str(), command_->cmd.length())) < 0)
    {
        LOGE("CommandSender send command error.\n");
        return -1;
    }

    LOGV("CommandSender SendCommand: %s\n", command_->cmd.c_str());
    return result;
}

int CommandSender::RecvCommand()
{
    int result;
    char buf[MAX_CMD_LENGTH] = {0};
    result = control_sock_->Recv(buf, sizeof(buf));
    // client disconnected.
    if(result <= 0 ) return -1;
    auto command = CommandFactory::New(buf);
    ASSERT_RETURN(command);
    if (is_stopping_)
    {
        is_stopping_ = false;
        is_stopped_ = true;
        LOGV("CommandSender recv result command.\n");
        ASSERT(is_stop_command_send_);
        auto result_command = std::dynamic_pointer_cast<ResultCommand>(command);
        ASSERT_RETURN(result_command, -1, "CommandSender recv result command error: %s", command->cmd);
        context_->ClrWriteFd(control_sock_->GetFd());
        context_->ClrReadFd(control_sock_->GetFd());
        return OnStop(result_command->netstat);
    }

    LOGV("CommandSender recv private command.\n");
    return OnRecvCommand(command);
}

int CommandSender::OnRecvCommand(std::shared_ptr<Command> command)
{
    return 0;
}

int CommandSender::Timeout(int timeout)
{
    ASSERT(timeout >= 0);
    timeout_ -= timeout;
    if (timeout_ <= 0)
        return OnTimeout();
    return 0;
}

EchoCommandSender::EchoCommandSender(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)),
      data_buf_(command_->GetSize(), 0),
      delay_(0), max_delay_(0), min_delay_(INT64_MAX),
      send_count_(0), recv_count_(0),
      CommandSender(channel)
{
}

int EchoCommandSender::OnStart()
{
    start_ = high_resolution_clock::now();
    return 0;
}

int EchoCommandSender::OnSendCommand()
{
    int result = CommandSender::OnSendCommand();
    ASSERT_RETURN(result>0,-1);
    context_->SetWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return result;
}

int EchoCommandSender::OnRecvCommand(std::shared_ptr<Command> command)
{
    // we don't expect recv any command
    LOGE("EchoCommandSender recv unexpected command: %s\n",command->cmd.c_str());
    ASSERT(0);
    return -1;
}

int EchoCommandSender::SendData()
{
    send_count_++;
    context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    
    begin_ = high_resolution_clock::now();
    if(data_buf_.size()<sizeof(int64_t)) data_buf_.resize(sizeof(int64_t));
    int64_t* buf = (int64_t*)&data_buf_[0];
    auto timestamp = begin_.time_since_epoch().count();
    // write timestamp to data
    *buf = timestamp;
    int result = data_sock_->Send(data_buf_.c_str(), data_buf_.length());
    stop_ = high_resolution_clock::now();
    return result;
}

int EchoCommandSender::RecvData()
{
    recv_count_++;
    //context_->SetWriteFd(data_fd_);
    //context_->ClrReadFd(data_sock_->GetFd());
    end_ = high_resolution_clock::now();
    int result = data_sock_->Recv(&data_buf_[0], data_buf_.length());
    if(result>=sizeof(int64_t))
    {
        int64_t* buf = (int64_t*)&data_buf_[0];
        auto delay = *buf - end_.time_since_epoch().count();
        max_delay_ = std::max(max_delay_, delay);
        min_delay_ = std::min(min_delay_, delay);
        delay_ = (delay_ + delay) / recv_count_;
    }
    else
    {
        ASSERT(0);
        return ERR_DEFAULT;
    }
    return result;
}

int EchoCommandSender::OnTimeout()
{
    if (send_count_ >= command_->GetCount())
    {
        return Stop();
    }
    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}

int EchoCommandSender::OnStop(std::shared_ptr<NetStat> stat)
{
    if (!OnStopped)
        return 0;
    stat = std::make_shared<NetStat>();
    stat->delay = delay_/(1000*1000);
    stat->max_delay = max_delay_/(1000*1000);
    stat->min_delay = min_delay_/(1000*1000);
    stat->jitter = stat->max_delay - stat->min_delay;
    stat->send_bytes = send_count_ * data_buf_.size();
    stat->send_packets = send_count_;
    stat->recv_packets = recv_count_;
    stat->loss = 1 - 1.0 * send_count_ / recv_count_;
    stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
    stat->send_speed = stat->send_bytes / duration_cast<duration<double>>(stop_ - start_).count();
    OnStopped(stat);
    return 0;
}

RecvCommandSender::RecvCommandSender(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<RecvCommandClazz>(channel->command_)),
      data_buf_(command_->GetSize(), 0),
      delay_(0), max_delay_(0), min_delay_(INT32_MAX),
      send_count_(0), recv_count_(0),
      is_stoping_(false),
      CommandSender(channel)
{
}

int RecvCommandSender::OnStart()
{
    start_ = high_resolution_clock::now();
    return 0;
}

int RecvCommandSender::OnSendCommand()
{
    int result = CommandSender::OnSendCommand();
    ASSERT_RETURN(result > 0, -1);
    context_->SetWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}
int RecvCommandSender::OnRecvCommand(std::shared_ptr<Command> command)
{
    // we don't expect recv any command
    LOGE("RecvCommandSender recv unexpected command: %s\n",command->cmd.c_str());
    ASSERT(0);
    return -1;
}
int RecvCommandSender::SendData()
{
    if (TryStop())
    {
        return Stop();
    }
    send_count_++;
    if (command_->GetInterval() > 0)
        context_->ClrWriteFd(data_sock_->GetFd());
    return data_sock_->Send(data_buf_.c_str(), data_buf_.length());
}
int RecvCommandSender::RecvData()
{
    // we don't expect recv any data
    ASSERT(0);
    return -1;
}
int RecvCommandSender::OnTimeout()
{
    if (TryStop())
    {
        return Stop();
    }

    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());

    return 0;
}
bool RecvCommandSender::TryStop()
{
    if (send_count_ >= command_->GetCount())
    {
        LOGV("begin stop.\n");
        context_->SetWriteFd(control_sock_->GetFd());
        context_->ClrWriteFd(data_sock_->GetFd());
        return true;
    }
    return false;
}
int RecvCommandSender::OnStop(std::shared_ptr<NetStat> netstat)
{
    stop_ = high_resolution_clock::now();
    is_stoping_ = false;
    if (!OnStopped)
        return 0;

    auto stat = std::make_shared<NetStat>();
    stat->send_bytes = send_count_ * data_buf_.size();
    stat->send_packets = send_count_;
    stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
    auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
    if (seconds > 0.001)
    {
        stat->send_speed = stat->send_bytes / seconds;
    }

    stat->recv_bytes = netstat->recv_bytes;
    stat->recv_packets = netstat->recv_packets;
    stat->recv_time = netstat->recv_time;
    stat->recv_speed = netstat->recv_speed;
    stat->min_recv_speed = netstat->min_recv_speed;
    stat->max_recv_speed = netstat->max_recv_speed;
    stat->loss = 1 - 1.0 * stat->recv_bytes / stat->send_bytes;

    LOGV("Run OnStop\n");
    OnStopped(stat);
    return 0;
}