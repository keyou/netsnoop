
#include <algorithm>

#include "command.h"
#include "command_receiver.h"

CommandReceiver::CommandReceiver(std::shared_ptr<CommandChannel> channel)
    : context_(channel->context_), control_sock_(channel->control_sock_),
      data_sock_(channel->data_sock_)
{
}

int CommandReceiver::RecvPrivateCommand(std::shared_ptr<Command> command)
{
    ASSERT_RETURN(0, -1, "CommandReceiver recv unexpected command: %s", command->cmd.c_str());
}

EchoCommandReceiver::EchoCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : send_count_(0), recv_count_(0), running_(false), is_stopping_(false),
      command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)), CommandReceiver(channel)
{
}

int EchoCommandReceiver::Start()
{
    LOGDP("EchoCommandReceiver start command.");
    ASSERT_RETURN(!running_, -1, "EchoCommandReceiver start unexpeted.");
    running_ = true;
    context_->SetReadFd(data_sock_->GetFd());
    return 0;
}
int EchoCommandReceiver::Stop()
{
    LOGDP("EchoCommandReceiver stop command.");
    ASSERT_RETURN(running_, -1, "EchoCommandReceiver stop unexpeted.");
    running_ = false;
    context_->ClrReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
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
    int result;
    ASSERT(running_);
    std::string buf(MAX_UDP_LENGTH, 0);
    if ((result = data_sock_->Recv(&buf[0], buf.length())) < 0)
    {
        return -1;
    }
    buf.resize(result);
    data_queue_.push(buf);
    context_->SetWriteFd(data_sock_->GetFd());
    // context_->ClrReadFd(data_sock_->GetFd());
    recv_count_++;
    return 0;
}

int EchoCommandReceiver::SendPrivateCommand()
{
    LOGDP("EchoCommandReceiver send stop");
    int result;
    context_->ClrWriteFd(control_sock_->GetFd());

    if (data_queue_.size() > 0)
    {
        LOGWP("echo stop: drop %ld data.", data_queue_.size());
    }

    auto command = std::make_shared<ResultCommand>();
    auto stat = std::make_shared<NetStat>();
    stat->recv_packets = recv_count_;
    stat->send_packets = send_count_;
    auto cmd = command->Serialize(*stat);
    if (OnStopped)
        OnStopped(command_, stat);
    if ((result = control_sock_->Send(cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return result;
}

SendCommandReceiver::SendCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : length_(0), recv_count_(0), recv_bytes_(0), speed_(0), min_speed_(-1), max_speed_(0),
      running_(false), is_stopping_(false), latest_recv_bytes_(0), token_(-1),
      illegal_packets_(0), reorder_packets_(0), duplicate_packets_(0),sequence_(0),
      command_(std::dynamic_pointer_cast<SendCommand>(channel->command_)), CommandReceiver(channel) {}

int SendCommandReceiver::Start()
{
    LOGDP("SendCommandReceiver start command.");
    ASSERT_RETURN(!running_, -1, "SendCommandReceiver start unexpeted.");
    running_ = true;
    context_->SetReadFd(data_sock_->GetFd());
    recv_count_ = 0;
    return 0;
}
int SendCommandReceiver::Stop()
{
    LOGDP("SendCommandReceiver stop command.");
    ASSERT_RETURN(running_, -1, "SendCommandReceiver stop unexpeted.");
    running_ = false;
    context_->ClrReadFd(data_sock_->GetFd());
    context_->ClrWriteFd(data_sock_->GetFd());
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
    int result;
    if ((result = data_sock_->Recv(buf_, sizeof(buf_))) < sizeof(DataHead))
    {
        LOGDP("recv data error.");
        return -1;
    }
    end_ = high_resolution_clock::now();
    stop_ = high_resolution_clock::now();
    auto head = (DataHead*)buf_;
    if (token_ == -1)
    {
        token_ = head->token;
    }
    else if (token_ != head->token)
    {
        illegal_packets_++;
        LOGWP("recv illegal data: seq=%ld, token=%c (expect %c)",head->sequence, head->token, token_);
    }
    else if(packets_.test(head->sequence))
    {
        duplicate_packets_++;
        LOGWP("recv duplicate data: seq=%ld, token=%c",head->sequence,head->token);
    }
    else if(head->sequence!=sequence_)
    {
        reorder_packets_++;
        sequence_ = head->sequence+1;
        LOGWP("recv reorder data: seq=%ld, token=%c",head->sequence,head->token);
    }
    if(head->token == token_)
    {
        packets_.set(head->sequence);
    }

    recv_bytes_ += result;
    recv_count_++;
    latest_recv_bytes_ += result;
    auto seconds = duration_cast<duration<double>>(end_ - begin_).count();
    if (seconds >= 1)
    {
        int64_t speed = latest_recv_bytes_ / seconds;
        min_speed_ = min_speed_ == -1 ? speed : std::min(min_speed_, speed);
        max_speed_ = std::max(max_speed_, speed);
        LOGIP("latest recv speed: recv_speed %d recv_bytes %ld recv_time %d", speed, latest_recv_bytes_, int(seconds * 1000));
        latest_recv_bytes_ = 0;
        begin_ = high_resolution_clock::now();
    }
    return result;
}

int SendCommandReceiver::SendPrivateCommand()
{
    LOGDP("SendCommandReceiver send stop");
    int result;
    context_->ClrWriteFd(control_sock_->GetFd());

    auto stat = std::make_shared<NetStat>();
    stat->recv_bytes = recv_bytes_;
    stat->recv_packets = recv_count_;
    stat->illegal_packets = illegal_packets_;
    stat->reorder_packets = reorder_packets_;
    stat->duplicate_packets = duplicate_packets_;
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
    if (OnStopped)
        OnStopped(command_, stat);
    if ((result = control_sock_->Send(cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return 0;
}
