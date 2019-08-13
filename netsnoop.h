#pragma once

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>


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
#define LOGW(...) fprintf(stderr, __VA_ARGS__);
#define LOGE(...) fprintf(stderr, __VA_ARGS__);

//#define LOGV(...)
//#define LOGW(...)
//#define LOGE(...)

#define EXPORT

#define ERR_ILLEGAL_PARAM -7
#define ERR_DEFAULT -1
#define ERR_SOCKET_CLOSED -2
#define ERR_TIMEOUT -3
#define MAX_CLINETS 500
#define ERR_OTHER -99
#define ERR_ILLEGAL_DATA -5

#define DISALLOW_COPY_AND_ASSIGN(clazz)       \
    clazz(const clazz &) = delete;            \
    clazz &operator=(const clazz &) = delete; 
    // clazz(clazz &&) = delete;                 \
    // clazz &operator=(clazz &&) = delete;

#ifdef _DEBUG
    #define ASSERT(condition) assert(condition)
    #define ASSERT_RETURN(condition,...) ASSERT(condition)
#else // NO _DEBUG
    #define ASSERT(x) 
    #define __ASSERT_RETURN1(condition) ASSERT(condition);return
    #define __ASSERT_RETURN2(condition,result) if(!(condition)) return (result)
    #define __ASSERT_RETURN3(condition,result,msg) if(!(condition)) { LOGE("%s\n",msg); return (result);}
    #define __ASSERT_RETURN4(condition,result,msg,data) if(!(condition)) { LOGE(msg,data); return (result);}

    #define __ASSERT_RETURN_SELECT(arg1,arg2,arg3,arg4,arg5,...) arg5
    #define __ASSERT_RETURN(...) __ASSERT_RETURN_SELECT(__VA_ARGS__,__ASSERT_RETURN4,__ASSERT_RETURN3,__ASSERT_RETURN2,__ASSERT_RETURN1)
    #define ASSERT_RETURN(...) __ASSERT_RETURN(__VA_ARGS__)(__VA_ARGS__)
#endif //_DEBUG

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