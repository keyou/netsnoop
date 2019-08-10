
#include "context2.h"
#include "action.h"

int EchoAction::Start()
{
    running_ = true;
    LOGV("Echo Start.%s\n", buf_);
    context_->SetReadFd(context_->data_fd);
    return 0;
}
int EchoAction::Stop()
{
    running_ = false;
    LOGV("Echo Stop.\n");
    context_->ClrReadFd(context_->data_fd);
    return 0;
}
int EchoAction::Send()
{
    LOGV("Echo Send.\n");
    int result;
    if (count_ <= 0)
        return 0;
    if ((result = Sock::Send(context_->data_fd, buf_, length_)) < 0)
    {
        return -1;
    }
    count_--;
    if (count_ <= 0)
    {
        context_->ClrWriteFd(context_->data_fd);
        if (running_)
            context_->SetReadFd(context_->data_fd);
    }
    return 0;
}
int EchoAction::Recv()
{
    LOGV("Echo Recv.\n");
    int result;
    if ((result = Sock::Recv(context_->data_fd, buf_, sizeof(buf_))) < 0)
    {
        return -1;
    }
    length_ = result;
    context_->SetWriteFd(context_->data_fd);
    context_->ClrReadFd(context_->data_fd);
    count_++;
    return 0;
}

int RecvAction::Start()
{
    LOGV("RecvAction Start.\n");
    context_->SetReadFd(context_->data_fd);
    return 0;
}
int RecvAction::Stop()
{
    LOGV("RecvAction Stop.\n");
    context_->ClrReadFd(context_->data_fd);
    return 0;
}
int RecvAction::Recv()
{
    int result;
    if ((result = Sock::Recv(context_->data_fd, buf_, sizeof(buf_))) < 0)
    {
        return -1;
    }
    return result;
}
