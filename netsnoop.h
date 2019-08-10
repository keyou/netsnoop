#pragma once

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>

#define ASSERT(x) assert(x)
#define RETURN_IF_NEG(x) if(x<0) return x;

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

#define ERR_ILLEGAL_PARAM -7
#define ERR_TIMEOUT -2
#define MAX_CLINETS 500
#define ERR_OTHER -99
#define ERR_ILLEGAL_DATA -5

#define DISALLOW_COPY_AND_ASSIGN(clazz)       \
    clazz(const clazz &) = delete;            \
    clazz &operator=(const clazz &) = delete; 
    // clazz(clazz &&) = delete;                 \
    // clazz &operator=(clazz &&) = delete;


void join_mcast(int fd, struct sockaddr_in *sin);

struct Option
{
    char ip_local[20];
    char ip_remote[20];
    int port;
    // Bit Rate
    int rate;
    // Buffer Size
    int buffer_size;
};