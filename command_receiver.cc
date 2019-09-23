
#include <algorithm>
#include <cmath>

#include "command.h"
#include "command_receiver.h"

CommandReceiver::CommandReceiver(std::shared_ptr<CommandChannel> channel)
    : context_(channel->context_), control_sock_(channel->control_sock_),
      data_sock_(channel->data_sock_)
{
}

int CommandReceiver::RecvPrivateCommand(std::shared_ptr<Command> command)
{
    ASSERT_RETURN(0, -1, "CommandReceiver recv unexpected command: %s", command->GetCmd().c_str());
}

EchoCommandReceiver::EchoCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : send_count_(0), recv_count_(0), running_(false), is_stopping_(false),
      command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)), CommandReceiver(channel),
      illegal_packets_(0), token_(command_->token)
{
}

int EchoCommandReceiver::Start()
{
    LOGDP("EchoCommandReceiver start command.");
    ASSERT_RETURN(!running_, -1, "EchoCommandReceiver start unexpeted.");
    running_ = true;
    //context_->SetReadFd(data_sock_->GetFd());
    return 0;
}
int EchoCommandReceiver::Stop()
{
    LOGDP("EchoCommandReceiver stop command.");
    ASSERT_RETURN(running_, -1, "EchoCommandReceiver stop unexpeted.");
    //context_->ClrReadFd(data_sock_->GetFd());
    //context_->ClrWriteFd(data_sock_->GetFd());
    // allow send result
    context_->SetWriteFd(control_sock_->GetFd());
    return 0;
}
int EchoCommandReceiver::Send()
{
    LOGVP("EchoCommandReceiver send payload.");
    ASSERT_RETURN(running_, -1, "EchoCommandReceiver send unexpeted.");
    int result = 0;
    context_->ClrWriteFd(data_sock_->GetFd());
    ASSERT(data_queue_.size() > 0);
    while (data_queue_.size() > 0)
    {
        auto buf = data_queue_.front();
        // use sync method to send extra data.
        if ((result = data_sock_->Send(&buf[0], buf.length())) < 0)
        {
            return -1;
        }
        send_count_++;
        data_queue_.pop();
    }

    return result;
}
int EchoCommandReceiver::Recv()
{
    LOGVP("EchoCommandReceiver recv payload.");
    ASSERT_RETURN(running_, -1, "EchoCommandReceiver recv unexpeted.");
    std::string buf(MAX_UDP_LENGTH, 0);
    int result = data_sock_->Recv(&buf[0], buf.length());
    buf.resize(std::max(result,0));
    if (result < sizeof(DataHead))
    {
        illegal_packets_++;
        LOGWP("recv illegal data(%d): length=%d, %s",data_sock_->GetFd(),result,Tools::GetDataSum(buf).c_str());
        return result;
    }
    
    auto head = reinterpret_cast<DataHead*>(&buf[0]);
    if (token_ != head->token)
    {
        illegal_packets_++;
        LOGWP("recv illegal data(%d): length=%d, seq=%ld, token=%c, expect %c",data_sock_->GetFd(),result,head->sequence, head->token, token_);
        return result;
    }

    data_queue_.push(buf);
    context_->SetWriteFd(data_sock_->GetFd());
    // context_->ClrReadFd(data_sock_->GetFd());
    recv_count_++;

    LOGDP("recv payload data: recv_count %ld seq %ld timestamp %ld token %c",recv_count_,head->sequence,head->timestamp,head->token);
    return result;
}

int EchoCommandReceiver::SendPrivateCommand()
{
    LOGDP("EchoCommandReceiver send stop");
    int result;
    if (data_queue_.size() > 0)
    {
        LOGWP("final send %ld data.", data_queue_.size());
        //TODO: optimize the logic
        Send();
    }
    context_->ClrWriteFd(control_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
    running_ = false;

    auto command = std::make_shared<ResultCommand>();
    auto stat = std::make_shared<NetStat>();
    stat->recv_packets = recv_count_;
    stat->send_packets = send_count_;
    stat->illegal_packets = illegal_packets_ + out_of_command_packets_;
    auto cmd = command->Serialize(*stat);
    LOGDP("command finish: %s || %s", command_->GetCmd().c_str(),cmd.c_str());
    if (OnStopped)
        OnStopped(command_, stat);
    if ((result = control_sock_->Send(cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return result;
}

SendCommandReceiver::SendCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : command_(std::dynamic_pointer_cast<SendCommand>(channel->command_)),
      buf_(MAX_UDP_LENGTH,'\0'),recv_count_(0), recv_bytes_(0), speed_(0), min_speed_(-1), max_speed_(0),
      running_(false), is_stopping_(false), latest_recv_bytes_(0), 
      illegal_packets_(0), reorder_packets_(0), duplicate_packets_(0), timeout_packets_(0), sequence_(0),
      token_(command_->token),
      CommandReceiver(channel) {}

int SendCommandReceiver::Start()
{
    LOGDP("SendCommandReceiver start command.");
    ASSERT_RETURN(!running_, -1, "SendCommandReceiver start unexpeted.");
    running_ = true;
    //context_->SetReadFd(data_sock_->GetFd());
    return 0;
}
int SendCommandReceiver::Stop()
{
    LOGDP("SendCommandReceiver stop command.");
    ASSERT_RETURN(running_, -1, "SendCommandReceiver stop unexpeted.");
    //context_->ClrReadFd(data_sock_->GetFd());
    //context_->ClrWriteFd(data_sock_->GetFd());
    //context_->ClrReadFd(control_sock_->GetFd());
    // allow to send stop command.
    context_->SetWriteFd(control_sock_->GetFd());
    return 0;
}
int SendCommandReceiver::Recv()
{
    LOGVP("SendCommandReceiver recv payload.");
    ASSERT_RETURN(running_, -1, "SendCommandReceiver recv unexpeted.");
    if (recv_count_ == 0)
    {
        start_ = high_resolution_clock::now();
        begin_ = high_resolution_clock::now();
    }
    int result = data_sock_->Recv(&buf_[0], buf_.length());
    buf_.resize(std::max(result,0));
    if (result < sizeof(DataHead))
    {
        illegal_packets_++;
        LOGWP("recv illegal data(%d): length=%d, %s",data_sock_->GetFd(),result,Tools::GetDataSum(buf_).c_str());
        return result;
    }

    auto head = reinterpret_cast<DataHead*>(&buf_[0]);
    if (token_ != head->token)
    {
        illegal_packets_++;
        LOGWP("recv illegal data(%d): length=%d, seq=%ld, token=%c, expect %c",data_sock_->GetFd(),result,head->sequence, head->token, token_);
        return result;
    }

    if(packets_.test(head->sequence))
    {
        duplicate_packets_++;
        LOGWP("recv duplicate data: seq=%ld",head->sequence,head->token);
        return result;
    }

    end_ = high_resolution_clock::now();
    stop_ = high_resolution_clock::now();

    LOGDP("recv payload data: recv_count %ld seq %ld expect_seq %ld timestamp %ld token %c",recv_count_,head->sequence,sequence_,head->timestamp,head->token);

    recv_bytes_ += result;
    recv_count_++;
    latest_recv_bytes_ += result;
    
    if(head->sequence!=sequence_)
    {
        reorder_packets_++;
        LOGWP("recv reorder data: seq=%ld, expect %ld",head->sequence,sequence_);
    }
    
    auto jump_count = int(head->sequence) - sequence_;
    if((jump_count>0 && jump_count<MAX_SEQ/2) || jump_count<0-MAX_SEQ/2)
    {
        // reset the locations which conrespond to the loss packets
        while (sequence_ != head->sequence)
        {
            packets_.reset((sequence_+MAX_SEQ/2)%MAX_SEQ);
            //LOGVP("reset %ld",(sequence_+MAX_SEQ/2)%MAX_SEQ);
            sequence_++;
        }
    }
    packets_.set(head->sequence);
    // cycle reset to reuse sequence
    packets_.reset((head->sequence+MAX_SEQ/2)%MAX_SEQ);
    //LOGVP("reset %ld",(head->sequence+MAX_SEQ/2)%MAX_SEQ);
    
    sequence_ = head->sequence+1;
    while (packets_.test(sequence_))
    {
        sequence_++;
    }
    
    auto time_delay = end_.time_since_epoch().count() - head->timestamp;
    if(recv_count_ == 1)
    {
        head_avg_delay_ = avg_delay_ = max_delay_ = min_delay_ = time_delay;
        //LOGDP("time_gap= %ld",time_delay);
    }

    if(time_delay - min_delay_ > command_->GetTimeout()*1000*1000)
    {
        timeout_packets_++;
    }
    
    total_delay_ += time_delay;
    
    max_delay_ = std::max(max_delay_,time_delay);
    min_delay_ = std::min(min_delay_,time_delay);
    auto old_avg_delay_ = avg_delay_;
    avg_delay_ = total_delay_/recv_count_;

    // The first 100 packets' delay may be more pure than all packets'.
    if(recv_count_<=100)
    {
        head_avg_delay_ = avg_delay_;
    }

    varn_delay_ = varn_delay_ + (time_delay - head_avg_delay_)*(time_delay- head_avg_delay_);
    std_delay_ = std::sqrt(varn_delay_/recv_count_);
    
    auto seconds = duration_cast<duration<double>>(end_ - begin_).count();
    if (seconds >= 1)
    {
        int64_t speed = latest_recv_bytes_ / seconds;
        min_speed_ = min_speed_ == -1 ? speed : std::min(min_speed_, speed);
        max_speed_ = std::max(max_speed_, speed);
        LOGIP("latest recv speed: recv_speed %ld recv_bytes %ld recv_time %d", speed, latest_recv_bytes_, int(seconds * 1000));
        latest_recv_bytes_ = 0;
        begin_ = high_resolution_clock::now();
    }
    #define D(x) (x-min_delay_)/1000/1000
    LOGDP("latest recv delay: recv_count %d delay %ld head_avg_delay %ld avg_delay %ld std_delay %ld max_delay %ld",recv_count_,D(time_delay),D(head_avg_delay_),D(avg_delay_),std_delay_,D(max_delay_));
    #undef D
    
    return result;
}

int SendCommandReceiver::SendPrivateCommand()
{
    LOGDP("SendCommandReceiver send stop");
    int result;
    context_->ClrWriteFd(control_sock_->GetFd());
    running_ = false;

    auto stat = std::make_shared<NetStat>();
    stat->recv_bytes = recv_bytes_;
    stat->recv_packets = recv_count_;
    stat->illegal_packets = illegal_packets_ + out_of_command_packets_;
    stat->reorder_packets = reorder_packets_;
    stat->duplicate_packets = duplicate_packets_;
    stat->timeout_packets = timeout_packets_;
    // use the min delay as time gap
    stat->delay = (avg_delay_-min_delay_)/1000/1000;
    stat->max_delay = (max_delay_-min_delay_)/1000/1000;
    stat->min_delay = 0;
    // use the head_avg_delay as jitter, because min_delay is always zero
    stat->jitter = (head_avg_delay_-min_delay_)/1000/1000;
    stat->jitter_std = std_delay_/1000/1000;
    auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
    if (seconds >= 0.001)
    {
        stat->recv_time = seconds * 1000;
        stat->recv_speed = recv_bytes_ / seconds;
        stat->recv_pps = recv_count_ / seconds;
        stat->max_recv_speed = max_speed_;
        if (min_speed_ > 0)
            stat->min_recv_speed = min_speed_;
    }

    auto command = std::make_shared<ResultCommand>();
    auto cmd = command->Serialize(*stat);
    LOGDP("command finish: %s || %s", command_->GetCmd().c_str(),cmd.c_str());
    if (OnStopped)
        OnStopped(command_, stat);
    if ((result = control_sock_->Send(cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return 0;
}
