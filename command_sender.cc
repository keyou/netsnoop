
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
      is_stopping_(false), is_stopped_(false), is_waiting_result_(false),
      is_starting_(false), is_started_(false),is_waiting_ack_(false)
{
}

int CommandSender::Start()
{
    LOGDP("CommandSender start command.");
    is_starting_ = true;
    // allow control channel send command
    context_->SetWriteFd(control_sock_->GetFd());
    return 0;
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
    LOGDP("CommandSender stop command.");
    ASSERT(!is_stopping_);
    is_stopping_ = true;
    // allow to send stop command
    //context_->SetWriteFd(control_sock_->GetFd());
    //timeout_ = -1;
    ASSERT(command_);
    // wait to allow client receive all data
    SetTimeout(command_->GetWait());
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
    ASSERT_RETURN(!is_stopped_,-1,"CommandSender has already stopped.");
    if (is_starting_)
    {
        is_starting_ = false;
        is_waiting_ack_ = true;
        LOGDP("CommandSender send command: %s", command_->GetCmd().c_str());
        if ((result = control_sock_->Send(command_->GetCmd().c_str(), command_->GetCmd().length())) < 0)
        {
            LOGEP("CommandSender send command error.");
            return -1;
        }
        return result;
    }
    if (is_stopping_)
    {
        ASSERT(is_started_);
        ASSERT(!is_waiting_result_);
        is_stopping_ = false;
        is_waiting_result_ = true;
        LOGDP("CommandSender send stop for: %s", command_->GetCmd().c_str());
        auto stop_command = std::make_shared<StopCommand>();
        result = control_sock_->Send(stop_command->GetCmd().c_str(), stop_command->GetCmd().length());
        if(result <= 0) return -1;
        return result;
    }
    return OnSendCommand();
}

int CommandSender::OnSendCommand()
{
    ASSERT_RETURN(0,-1,"CommandSender has no command to send.");
}

int CommandSender::RecvCommand()
{
    int result;
    char buf[MAX_CMD_LENGTH] = {0};
    result = control_sock_->Recv(buf, sizeof(buf));
    // client disconnected.
    if(result<=0) return -1;
    auto command = CommandFactory::New(buf);
    ASSERT_RETURN(command,-1);
    if (is_waiting_result_)
    {
        is_waiting_result_ = false;
        is_stopped_ = true;
        auto result_command = std::dynamic_pointer_cast<ResultCommand>(command);
        ASSERT_RETURN(result_command, -1, "CommandSender expect recv result command: %s", command->GetCmd().c_str());
        LOGDP("CommandSender recv result: %s",result_command->netstat->ToString().c_str());
        // should not clear control sock,keep control sock readable for detecting client disconnect
        //context_->ClrReadFd(control_sock_->GetFd());
        return OnStop(result_command->netstat);
    }

    if(is_waiting_ack_)
    {
        is_waiting_ack_ = false;
        is_started_ = true;
        auto ack_command = std::dynamic_pointer_cast<AckCommand>(command);
        ASSERT_RETURN(ack_command, -1, "CommandSender expect recv ack command: %s", command->GetCmd().c_str());
        return OnStart();
    }

    LOGDP("CommandSender recv private command.");
    return OnRecvCommand(command);
}

int CommandSender::OnRecvCommand(std::shared_ptr<Command> command)
{
    ASSERT_RETURN(0,-1,"CommandSender recv unexpected command: %s",command?command->GetCmd().c_str():"NULL");
}

int CommandSender::Timeout(int timeout)
{
    ASSERT(timeout > 0);
    timeout_ -= timeout;
    if (timeout_ <= 0)
    {
        if(is_stopping_)
        {
            // allow to send stop command
            context_->SetWriteFd(control_sock_->GetFd());
        }
        else return OnTimeout();
    }
    return 0;
}

#pragma region EchoCommandSender

EchoCommandSender::EchoCommandSender(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)),
      data_buf_(command_->GetSize(), command_->token),
      delay_(0), max_delay_(0), min_delay_(0),
      send_packets_(0), recv_packets_(0),illegal_packets_(0),
      CommandSender(channel)
{
    if(data_buf_.size()<sizeof(DataHead)) data_buf_.resize(sizeof(DataHead));
}

int EchoCommandSender::OnStart()
{
    LOGDP("EchoCommandSender start payload.");
    start_ = high_resolution_clock::now();
    context_->SetWriteFd(data_sock_->GetFd());
    //context_->ClrReadFd(data_sock_->GetFd());
    //context_->SetReadFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}

int EchoCommandSender::SendData()
{
    send_packets_++;
    //context_->SetReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    begin_ = high_resolution_clock::now();
    auto head = (DataHead*)&data_buf_[0];
    auto timestamp = begin_.time_since_epoch().count();
    // write timestamp to data
    head->timestamp = timestamp;
    head->sequence = send_packets_-1;
    head->token = command_->token;
    int result = data_sock_->Send(data_buf_.c_str(), data_buf_.length());
    if(result<0)
    {
        LOGEP("send payload error(%d).",data_sock_->GetFd());
    }
    else
    {
        LOGDP("send payload data: send_packets %ld seq %ld timestamp %ld token %c",send_packets_,head->sequence,head->timestamp,head->token);
    }
    return result;
}

int EchoCommandSender::RecvData()
{
    end_ = high_resolution_clock::now();
    int result = data_sock_->Recv(&data_buf_[0], data_buf_.length());
    auto head = (DataHead*)&data_buf_[0];
    if(result<sizeof(DataHead)||head->token!=command_->token)
    {
        LOGWP("recv illegal data(%d): length=%d, %s",data_sock_->GetFd(),result,Tools::GetDataSum(data_buf_.substr(0,result>0?std::min(result,64):0)).c_str());
        illegal_packets_++;
        return result;
    }

    recv_packets_++;
    auto delay = end_.time_since_epoch().count() - head->timestamp;
    if(recv_packets_ == 1)
    {
        max_delay_ = min_delay_ = delay;
    }
    if(delay>command_->GetTimeout()*1000*1000)
    {
        timeout_packets_++;
    }

    max_delay_ = std::max(max_delay_, delay);
    min_delay_ = std::min(min_delay_, delay);
    auto old_delay = delay_;
    delay_ = delay_ + (delay - delay_)/recv_packets_;
    varn_delay_ = varn_delay_ + (delay - old_delay)*(delay-delay_);
    std_delay_ = std::sqrt(varn_delay_/recv_packets_);

    LOGDP("recv payload data: recv_packets %ld seq %ld timestamp %ld token %c",recv_packets_,head->sequence,head->timestamp,head->token);
    LOGIP("ping delay %.02f",delay/1000.0/1000);

    return result;
}

int EchoCommandSender::OnTimeout()
{
    if (send_packets_ >= command_->GetCount())
    {
        stop_ = high_resolution_clock::now();
        // stop data channel
        // context_->ClrWriteFd(data_sock_->GetFd());
        return Stop();
    }
    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());
    return 0;
}

int EchoCommandSender::OnStop(std::shared_ptr<NetStat> netstat)
{
    LOGDP("EchoCommandSender stop payload.");
    //context_->ClrReadFd(data_sock_->GetFd());
    if (!OnStopped)
        return 0;
    
    auto stat = std::make_shared<NetStat>();
    stat->delay = delay_/(1000*1000);
    stat->max_delay = max_delay_/(1000*1000);
    stat->min_delay = min_delay_/(1000*1000);
    stat->jitter = stat->max_delay - stat->min_delay;
    stat->jitter_std = std_delay_/(1000*1000);
    stat->send_bytes = send_packets_ * data_buf_.size();
    stat->recv_bytes = recv_packets_ * data_buf_.size();
    stat->send_packets = send_packets_;
    stat->recv_packets = recv_packets_;
    stat->illegal_packets = illegal_packets_;
    stat->timeout_packets = timeout_packets_;
    stat->loss = 1 - 1.0 * recv_packets_ / send_packets_;
    stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
    auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
    if(stat->send_time>=1)
    {
        stat->send_pps = send_packets_/ seconds;
        stat->recv_pps = recv_packets_/ seconds;
        stat->send_speed = stat->send_bytes / seconds;
        stat->recv_speed = stat->recv_bytes / seconds;
    }
    OnStopped(stat);
    return 0;
}

#pragma endregion

SendCommandSender::SendCommandSender(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<SendCommandClazz>(channel->command_)),
      data_buf_(command_->GetSize(), command_->token),
      send_packets_(0), send_bytes_(0),is_stoping_(false),
      CommandSender(channel)
{
    if(data_buf_.size()<sizeof(DataHead)) data_buf_.resize(sizeof(DataHead));
}

int SendCommandSender::OnStart()
{
    LOGDP("SendCommandSender start payload.");
    //context_->ClrReadFd(data_sock_->GetFd());
    if(!command_->is_multicast)
        context_->SetWriteFd(data_sock_->GetFd());
    //SetTimeout(command_->GetInterval());
    return 0;
}

int SendCommandSender::SendData()
{
    if(command_->is_multicast && is_stoping_) return 0;
    if (TryStop())
    {
        LOGDP("SendCommandSender stop from send data.");
        is_stoping_ = true;
        // stop data channel
        context_->ClrWriteFd(data_sock_->GetFd());
        return Stop();
    }
    LOGDP("SendCommandSender send payload data.");
    if(start_.time_since_epoch().count() == 0)
    {
        start_ = high_resolution_clock::now();
        SetTimeout(command_->GetInterval());
    }
    if (command_->GetInterval() > 0)
        context_->ClrWriteFd(data_sock_->GetFd());
    
    DataHead* head = (DataHead*)&data_buf_[0];
    head->timestamp = high_resolution_clock::now().time_since_epoch().count();
    head->sequence = send_packets_;
    head->token = command_->token;

    int result = data_sock_->Send(data_buf_.c_str(), data_buf_.length());
    if(result<0)
    {
        LOGEP("send payload error(%d).",data_sock_->GetFd());
    }
    else if(result>0)
    {
        send_packets_++;
        send_bytes_+=result;
        stop_ = high_resolution_clock::now();
    }
    return result;
}
int SendCommandSender::RecvData()
{
    // we don't expect recv any data
    std::string buf(MAX_UDP_LENGTH,'\0');
    int result = data_sock_->Recv(&buf[0], buf.length());
    buf.resize(std::max(result,0));
    auto head = (DataHead*)&buf[0];
    if(result < sizeof(DataHead))
    {
        LOGWP("recv illegal data(%d): length=%d, %s",data_sock_->GetFd(),result,Tools::GetDataSum(buf).c_str());
        return result;
    }
    LOGWP("recv illegal data(%d): length=%d, seq %s timestamp %ld token %c result %d",data_sock_->GetFd(),result,head->sequence,head->timestamp,head->token);
    return 0;
}
int SendCommandSender::OnTimeout()
{
    if(is_stoping_) return 0;
    if (TryStop())
    {
        LOGDP("SendCommandSender stop from timeout.");
        is_stoping_ = true;
        // stop data channel
        context_->ClrWriteFd(data_sock_->GetFd());
        return Stop();
    }

    context_->SetWriteFd(data_sock_->GetFd());
    SetTimeout(command_->GetInterval());

    return 0;
}
bool SendCommandSender::TryStop()
{
    if (send_packets_ >= command_->GetCount()||command_->is_finished)
    {
        return true;
    }
    return false;
}
int SendCommandSender::OnStop(std::shared_ptr<NetStat> netstat)
{
    LOGDP("SendCommandSender stop payload.");
    if (!OnStopped)
        return 0;

    auto stat = netstat;
    if(send_packets_>0)
    {
        stat->send_bytes = send_bytes_;
        stat->send_packets = send_packets_;
        stat->send_time = duration_cast<milliseconds>(stop_ - start_).count();
        auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
        if (seconds > 0.001)
        {
            stat->send_pps = send_packets_/ seconds;
            stat->send_speed = stat->send_bytes / seconds;
        }
        stat->loss = 1 - 1.0 * stat->recv_packets / send_packets_;
    }

    OnStopped(stat);
    return 0;
}
