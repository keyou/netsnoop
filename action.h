#pragma once

#include "context2.h"
#include "sock.h"

class Action
{
public:
    Action(std::shared_ptr<Context> context) : context_(context) {}
    Action(std::string argv, std::shared_ptr<Context> context)
        : argv_(argv), context_(context) {}
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual int Send() { return 0; };
    virtual int Recv() { return 0; };

protected:
    std::string argv_;
    std::shared_ptr<Context> context_;
};

class EchoAction : public Action
{
public:
    EchoAction(std::shared_ptr<Context> context)
        : length_(0), count_(0), running_(false), Action(context) {}

    void Start() override;
    void Stop() override;
    int Send() override;
    int Recv() override;
    
private:
    char buf_[1024 * 64];
    int length_;
    ssize_t count_;
    bool running_;
};
