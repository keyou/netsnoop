#pragma once

#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>

#include <ctime>
#include <ratio>
#include <chrono>

#include <memory>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <functional>

#include "netsnoop.h"
#include "udp.h"

using namespace std::chrono;

#define ASSERT(x) assert(x)

class Logger
{
public:
    Logger(const std::string &level)
    {
        level_ = level;
    }
    Logger &operator<<(const std::string &log)
    {
        log_ += log;
    }

private:
    std::string level_;
    std::string log_;
    ~Logger()
    {
        std::clog << log_ << std::endl;
    }
};

#define TAG "NETSNOOP"
#define LOGV(...) fprintf(stdout, __VA_ARGS__)
#define LOGW(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
//#define LOGV(...)
//#define LOGW(...)
//#define LOGE(...)

#define EXPORT

enum CommandType : char
{
    CMD_NULL = 0,
    CMD_RECV = 1,
    CMD_SEND = 2,
    CMD_ECHO = 3
};

class Peer;

struct Context
{
    Context():max_fd(-1),control_fd(-1),data_fd(-1)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
    }

    inline void SetReadFd(int fd)
    {
        FD_SET(fd,&read_fds);
        max_fd = std::max(max_fd,fd);
    }

    inline void SetWriteFd(int fd)
    {
        FD_SET(fd,&write_fds);
        max_fd = std::max(max_fd,fd);
    }

    inline void ClrReadFd(int fd)
    {
        FD_CLR(fd,&read_fds);
    }

    inline void ClrWriteFd(int fd)
    {
        FD_CLR(fd,&write_fds);
    }

    ~Context()
    {
        if(control_fd>0) 
        {
            close(control_fd);
            control_fd = -1;
        }
        if(data_fd>0) 
        {
            close(data_fd);
            data_fd = -1;
        }
    }

    int control_fd;
    int data_fd;
    fd_set read_fds;
    fd_set write_fds;
    int max_fd;
    timespec timeout;
    std::function<void()> timeout_callback;
    std::vector<std::shared_ptr<Peer>> peers;
};

const std::string echo_cmd("ECHO");
const std::string recv_cmd("RECV");
const std::string send_cmd("SEND 1024 10");

class Peer
{
public:
    Peer(int control_fd,const std::string cookie,std::shared_ptr<Context> context):
        control_fd_(control_fd),data_fd_(-1),cookie_(cookie),context_(context),cmd_(CMD_NULL)
    {
    }
    
    inline void SetDataFd(int data_fd){data_fd_ = data_fd;}
    inline int GetDataFd(){return data_fd_;}
    
    int SendCommand()
    {
        if(cmd_ == CMD_NULL)
        {
            if(sock_send(control_fd_,echo_cmd.c_str(),echo_cmd.length()) == -1)
            {
                LOGE("change mode error.\n");
                close(control_fd_);
                return -1;
            }
            context_->SetWriteFd(data_fd_);
            context_->ClrReadFd(data_fd_);    
            cmd_=CMD_ECHO;
            context_->timeout.tv_nsec = 900*1000*1000; //100ms
            context_->timeout_callback =[&](){
                context_->SetWriteFd(data_fd_);
            };
        }
        // Stop send control cmd and Start recv control cmd.
        context_->ClrWriteFd(control_fd_);
        context_->SetReadFd(control_fd_);
        return 0;
    }
    int RecvCommand()
    {
        int result;
        char buf[1024*64];
        if((result = sock_recv(control_fd_,buf,sizeof(buf)))<=0)
        {
            LOGE("Disconnect.\n");
            context_->ClrReadFd(control_fd_);
            context_->ClrReadFd(data_fd_);
            context_->ClrWriteFd(control_fd_);
            context_->ClrWriteFd(data_fd_);
            context_->timeout.tv_nsec = 0;
            //context_->peers.erase(std::remove(context_->peers.begin(),context_->peers.end(),[](){return true;}),context_->peers.end());
        }
        return 0;
    }

    int SendData()
    {
        if(cmd_ == CMD_ECHO) return echo_send();
        if(cmd_ == CMD_RECV) return echo_send();
        if(cmd_ == CMD_SEND) return echo_send();
        LOGE("Peer send error: cmd = %d\n",cmd_);
        #define ERR_OTHER -99
        return ERR_OTHER;
    }
    int RecvData()
    {        
        if(cmd_ == CMD_ECHO) return echo_recv();
        if(cmd_ == CMD_RECV) return echo_recv();
        if(cmd_ == CMD_SEND) return echo_recv();
        LOGE("Peer recv error: cmd = %d\n",cmd_);
        #define ERR_OTHER -99
        return ERR_OTHER;
    }
    int echo_send()
    {
        context_->SetReadFd(data_fd_);
        context_->ClrWriteFd(data_fd_);
        const std::string tmp(10,'a');
        return sock_send(data_fd_,tmp.c_str(),tmp.length());
    }
    int echo_recv()
    {
        //context_->SetWriteFd(data_fd_);
        context_->ClrReadFd(data_fd_);
        return sock_recv(data_fd_,buf_,sizeof(buf_));
    }
    int GetControlFd(){return control_fd_;}
    int GetCmd(){return cmd_;}
    const std::string& GetCookie(){return cookie_;}
private:
    Peer(const Peer& peer){}
    void operator= (const Peer& p){}
    int control_fd_;
    int data_fd_;
    std::string cookie_;
    int cmd_;
    char buf_[1024*64];
    std::shared_ptr<Context> context_;
};