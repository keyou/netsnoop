#pragma once

#include "context2.h"
#include "sock.h"

class CommandReceiver
{
public:
    CommandReceiver(std::shared_ptr<Context> context) : context_(context) {}
    CommandReceiver(std::string argv, std::shared_ptr<Context> context)
        : argv_(argv), context_(context) {}
    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int Send() { return 0; };
    virtual int Recv() { return 0; };

protected:
    std::string argv_;
    std::shared_ptr<Context> context_;
};

class EchoCommandReceiver : public CommandReceiver
{
public:
    EchoCommandReceiver(std::shared_ptr<Context> context)
        : length_(0),buf_{0}, count_(0), running_(false), CommandReceiver(context)
    {
    }

    int Start() override;
    int Stop() override;
    int Send() override;
    int Recv() override;

private:
    char buf_[1024 * 64];
    int length_;
    ssize_t count_;
    bool running_;
};

class RecvCommandReceiver : public CommandReceiver
{
public:
    RecvCommandReceiver(std::shared_ptr<Context> context)
        : length_(0), count_(0), running_(false), CommandReceiver(context) {}

    int Start() override;
    int Stop() override;
    int Recv() override;

private:
    char buf_[1024 * 64];
    int length_;
    ssize_t count_;
    bool running_;
};
