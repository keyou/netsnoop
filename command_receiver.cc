
#include <algorithm>
#include <sys/socket.h>

#include "command.h"
#include "command_receiver.h"

CommandReceiver::CommandReceiver(std::shared_ptr<CommandChannel> channel)
    : context_(channel->context_)
{
}

EchoCommandReceiver::EchoCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : send_count_(0),recv_count_(0), running_(false),is_stopping_(false),
      command_(std::dynamic_pointer_cast<EchoCommand>(channel->command_)), CommandReceiver(channel)
{
}

int EchoCommandReceiver::Start()
{
    running_ = true;
    LOGV("EchoCommandReceiver Start.\n");
    context_->SetReadFd(context_->data_fd);
    return 0;
}
int EchoCommandReceiver::Stop()
{
    running_ = false;
    LOGV("EchoCommandReceiver Stop.\n");
    context_->ClrReadFd(context_->data_fd);
    // allow send result
    context_->SetWriteFd(context_->control_fd);
    return 0;
}
int EchoCommandReceiver::Send()
{
    LOGV("EchoCommandReceiver Send.\n");
    int result;
    if (data_queue_.size() == 0)
    {
        context_->ClrWriteFd(context_->data_fd);
        if(is_stopping_)
        {
            return Stop();
        }
        return 0;
    }
    auto buf = data_queue_.front();
    if ((result = Sock::Send(context_->data_fd, &buf[0], buf.length())) < 0)
    {
        return -1;
    }
    send_count_++;
    data_queue_.pop();
    return result;
}
int EchoCommandReceiver::Recv()
{
    LOGV("EchoCommandReceiver Recv.\n");
    int result;
    std::string buf(1024*64,0);
    if ((result = Sock::Recv(context_->data_fd, &buf[0], buf.length())) < 0)
    {
        return -1;
    }
    buf.resize(result);
    data_queue_.push(buf);
    context_->SetWriteFd(context_->data_fd);
    // context_->ClrReadFd(context_->data_fd);
    recv_count_++;
    return 0;
}
int EchoCommandReceiver::RecvPrivateCommand(std::shared_ptr<Command> private_command)
{
    auto command = std::dynamic_pointer_cast<StopCommand>(private_command);
    ASSERT_RETURN(command, -1, "recv command error: not stop command.");
    if(data_queue_.size()==0) 
    {
        return Stop();
    }
    is_stopping_ = true;
    return 0;
}

int EchoCommandReceiver::SendPrivateCommand()
{
    int result;
    context_->ClrWriteFd(context_->control_fd);
    auto command = std::make_shared<ResultCommand>();
    NetStat stat;
    stat.recv_packets = recv_count_;
    stat.send_packets = send_count_;
    auto cmd = command->Serialize(stat);
    if ((result = Sock::Send(context_->control_fd, cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return result;
}

RecvCommandReceiver::RecvCommandReceiver(std::shared_ptr<CommandChannel> channel)
    : length_(0), recv_count_(0), recv_bytes_(0), speed_(0), min_speed_(-1), max_speed_(0), running_(false),
      command_(std::dynamic_pointer_cast<RecvCommand>(channel->command_)), CommandReceiver(channel) {}

int RecvCommandReceiver::Start()
{
    LOGV("RecvCommandReceiver Start.\n");
    context_->SetReadFd(context_->data_fd);
    start_ = high_resolution_clock::now();
    begin_ = high_resolution_clock::now();
    return 0;
}
int RecvCommandReceiver::Stop()
{
    LOGV("RecvCommandReceiver Stop.\n");
    context_->ClrReadFd(context_->data_fd);
    return 0;
}
int RecvCommandReceiver::Recv()
{
    LOGV("RecvCommandReceiver Recv.\n");
    int result;
    if ((result = Sock::Recv(context_->data_fd, buf_, sizeof(buf_))) < 0)
    {
        return -1;
    }
    end_ = high_resolution_clock::now();
    stop_ = high_resolution_clock::now();
    recv_bytes_ += result;
    recv_count_++;
    auto seconds = duration_cast<duration<double>>(end_ - begin_).count();
    if (seconds >= 1)
    {
        int64_t speed = recv_bytes_ / seconds;
        min_speed_ = min_speed_ == -1 ? speed : std::min(min_speed_, speed);
        max_speed_ = std::max(max_speed_, speed);
        begin_ = high_resolution_clock::now();
    }
    return result;
}
int RecvCommandReceiver::RecvPrivateCommand(std::shared_ptr<Command> private_command)
{
    auto command = std::dynamic_pointer_cast<StopCommand>(private_command);
    ASSERT_RETURN(command, -1, "recv command error: not stop command.");
    context_->SetWriteFd(context_->control_fd);
    return 0;
}

int RecvCommandReceiver::SendPrivateCommand()
{
    int result;
    char buf = 0;
    result = recv(context_->data_fd, &buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
    if(result != -1)
    {
        LOGV("client has more data to read.\n");
        return 0;
    }
    context_->ClrWriteFd(context_->control_fd);
    //TODO: sync control and data channel

    NetStat stat = {};
    stat.recv_bytes = recv_bytes_;
    stat.recv_packets = recv_count_;
    auto seconds = duration_cast<duration<double>>(stop_ - start_).count();
    if (seconds >= 0.001)
    {
        stat.recv_time = seconds * 1000;
        stat.recv_speed = recv_bytes_ / seconds;
        stat.max_recv_speed = max_speed_;
        if (min_speed_ > 0)
            stat.min_recv_speed = min_speed_;
    }

    auto command = std::make_shared<ResultCommand>();
    auto cmd = command->Serialize(stat);
    if ((result = Sock::Send(context_->control_fd, cmd.c_str(), cmd.length())) < 0)
    {
        return -1;
    }
    return 0;
}