
#include <cmath>
#include <functional>

#include "command.h"
#include "netsnoop.h"
#include "udp.h"
#include "tcp.h"
#include "context2.h"
#include "peer.h"
#include "command_sender.h"

CommandSender::CommandSender(std::shared_ptr<CommandChannel> channel)
    : timeout_(-1), control_sock_(channel->control_sock_), data_sock_(channel->data_sock_),
      context_(channel->context_), command_(channel->command_)
{
}

int CommandSender::SendCommand()
{
    int result;
    if ((result = control_sock_->Send(command_->cmd.c_str(), command_->cmd.length())) == -1)
    {
        LOGE("change commandsender error.\n");
        return -1;
    }
    LOGV("CommandSender SendCommand: %s\n", command_->cmd.c_str());
    return result;
}

int CommandSender::Timeout(int timeout)
{
    ASSERT(timeout>=0);
    timeout_ -= timeout;
    if (timeout_ <= 0)
        return OnTimeout();
    return 0;
}

int CommandSender::SendStop()
{
    auto stop_command = std::make_shared<StopCommand>();
    return control_sock_->Send(stop_command->cmd.c_str(),stop_command->cmd.length());
}

EchoCommandSender::EchoCommandSender(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)),
      data_buf_(command_->GetSize(), 0),
      delay_(0), max_delay_(0), min_delay_(INT32_MAX),
      send_count_(0), recv_count_(0),
      CommandSender(channel)
{
}

int EchoCommandSender::SendCommand()
{
    start_ = high_resolution_clock::now();
    CommandSender::SendCommand();
    context_->SetWriteFd(data_sock_->GetFd());
    context_->ClrReadFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}

int EchoCommandSender::RecvCommand()
{
    int result;
    char buf_[MAX_CMD_LENGTH] = {0};
    result = control_sock_->Recv(&buf_[0], sizeof(buf_));
    ASSERT_RETURN(result > 0, -1);
    //TODO: deal with the command
    return 0;
}

int EchoCommandSender::SendData()
{
    send_count_++;
    context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    begin_ = high_resolution_clock::now();
    return data_sock_->Send(data_buf_.c_str(), data_buf_.length());
}

int EchoCommandSender::RecvData()
{
    recv_count_++;
    //context_->SetWriteFd(data_fd_);
    //context_->ClrReadFd(data_sock_->GetFd());
    end_ = high_resolution_clock::now();
    double delay = duration_cast<duration<double>>(end_ - begin_).count() * 1000;
    max_delay_ = std::max(max_delay_, delay);
    min_delay_ = std::min(min_delay_, delay);
    delay_ = (delay_ + delay) / recv_count_;
    return data_sock_->Recv(&data_buf_[0], data_buf_.length());
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

int EchoCommandSender::Stop()
{
    stop_ = high_resolution_clock::now();
    if (!OnStop)
        return 0;
    auto stat = std::make_shared<NetStat>();
    stat->delay =delay_;
    stat->jitter = stat->max_delay - stat->min_delay;
    stat->send_bytes = send_count_ * data_buf_.size();
    stat->send_packets = send_count_;
    stat->recv_packets = recv_count_;
    stat->loss = 1 - 1.0 * send_count_ / recv_count_;
    stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
    stat->send_speed = stat->send_bytes / duration_cast<duration<double>>(stop_ - start_).count();
    OnStop(stat);
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

int RecvCommandSender::SendCommand()
{
    context_->ClrWriteFd(control_sock_->GetFd());
    if(is_stoping_)
    {
        LOGV("RecvCommandSender send stop command.\n");
        return SendStop();
    }
    int result = CommandSender::SendCommand();
    ASSERT_RETURN(result > 0, -1);
    start_ = high_resolution_clock::now();
    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}
int RecvCommandSender::RecvCommand()
{
    LOGV("RecvCommandSender recv result command.\n");
    int result;
    char buf[MAX_CMD_LENGTH] = {0};
    result = control_sock_->Recv(buf, sizeof(buf));
    ASSERT_RETURN(is_stoping_,-1,"RecvCommandSender recv command error: stop already.");
    ASSERT_RETURN(result > 0, -1,"RecvCommandSender recv command error: empty data");
    auto command = CommandFactory::New(buf);
    return Stop(command);
}
int RecvCommandSender::SendData()
{
    if(TryStop())
    {
        return 0;
    }
    send_count_++;
    if(command_->GetInterval()>0) context_->ClrWriteFd(data_sock_->GetFd());
    return data_sock_->Send(data_buf_.c_str(), data_buf_.length());
}
int RecvCommandSender::RecvData()
{
    return 0;
}
int RecvCommandSender::OnTimeout()
{
    if (TryStop())
    {
        return 0;
    }

    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());

    return 0;
}
bool RecvCommandSender::TryStop()
{
    if(send_count_>=command_->GetCount())
    {
        LOGV("begin stop.\n");
        stop_ = high_resolution_clock::now();
        is_stoping_ = true;
        context_->SetWriteFd(control_sock_->GetFd());
        context_->ClrWriteFd(data_sock_->GetFd());
        return true;
    }
    return false;
}
int RecvCommandSender::Stop(std::shared_ptr<Command> command)
{
    is_stoping_ = false;
    if (!OnStop)
        return 0;
    
    auto result_command = std::dynamic_pointer_cast<ResultCommand>(command);
    ASSERT_RETURN(result_command,-1,"RecvCommandSender recv command error: not result command");
    
    auto stat = std::make_shared<NetStat>();
    stat->send_bytes = send_count_ * data_buf_.size();
    stat->send_packets = send_count_;
    stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
    auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
    if(seconds>0.001)
    {
        stat->send_speed = stat->send_bytes / seconds;
    }
    
    stat->recv_bytes = result_command->netstat->recv_bytes;
    stat->recv_packets = result_command->netstat->recv_packets;
    stat->recv_time = result_command->netstat->recv_time;
    stat->recv_speed = result_command->netstat->recv_speed;
    stat->min_recv_speed = result_command->netstat->min_recv_speed;
    stat->max_recv_speed = result_command->netstat->max_recv_speed;
    stat->loss = 1 - 1.0 * stat->recv_bytes / stat->send_bytes;

    LOGV("Run OnStop\n");
    OnStop(stat);
    return 0;
}