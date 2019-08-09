
#include "context2.h"
#include "action.h"

void EchoAction::Start()
{
    running_ = true;
    LOGV("Echo Start.\n");
    context_->SetReadFd(context_->data_fd);
}
void EchoAction::Stop()
{
    running_ = false;
    LOGV("Echo Stop.\n");
    context_->ClrReadFd(context_->data_fd);
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

    void RecvAction::Start()
    {
        context_->SetReadFd(context_->data_fd);
    }
    void RecvAction::Stop()
    {
        context_->ClrReadFd(context_->data_fd);
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
