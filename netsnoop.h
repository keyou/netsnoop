#pragma once

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstdarg>
#include <stdarg.h>
#include <sstream>

enum LogLevel
{
    LLVERBOSE = 0,
    LLDEBUG = 1,
    LLINFO = 2,
    LLWARN = 3,
    LLERROR = 4
};

class Logger
{
public:
    Logger(LogLevel level=LLDEBUG)
    {
        switch (level)
        {
        case LLVERBOSE:
            out_ = &std::cout;
            *out_<<"[VER]";
            break;
        case LLDEBUG:
            out_ = &std::cout;
            *out_<<"[DBG]";
            break;
        case LLWARN:
            out_ = &std::cout;
            *out_<<"[WAR]";
            break;
        case LLERROR:
            out_ = &std::cerr;
            *out_<<"[ERR]";
            break;
        default:
            out_ = &std::cout;
            *out_<<"[INF]";
        }
    }
    Logger &operator<<(const std::string &log)
    {
        *out_<<log;
        return *this;
    }

    Logger& Print(const char* fmt,...)
    {
        using namespace std::chrono;

        auto tp = high_resolution_clock::now();
        auto tm = system_clock::to_time_t(tp);
        auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count()%1000;
        char buf[1024]={0};
        if (std::strftime(buf, sizeof(buf), "%m/%d %T", std::localtime(&tm))) {
            *out_ << "[" << buf <<"."<< ms <<"]";
        }

        *out_<<"["<<std::this_thread::get_id()<<"] ";
        
        va_list args;
        va_start(args, fmt);
        vsprintf(buf,fmt,args);
        va_end(args);

        *out_<<buf;
        return *this;
    }
    
    ~Logger()
    {
        *out_ << std::endl;
    }
private:
    std::ostream* out_;
};

#define TAG "NETSNOOP"
#ifdef _DEBUG
    #define LOGV(...) Logger(LLVERBOSE).Print(__VA_ARGS__);
    #define LOGW(...) Logger(LLWARN).Print(__VA_ARGS__);
    #define LOGE(...) Logger(LLERROR).Print(__VA_ARGS__);
#else // NO _DEBUG
    #define LOGV(...) //Logger(LLVERBOSE).Print(__VA_ARGS__);
    #define LOGW(...) Logger(LLWARN).Print(__VA_ARGS__);
    #define LOGE(...) Logger(LLERROR).Print(__VA_ARGS__);
#endif // _DEBUG

#ifdef _DEBUG
    #define ASSERT(expr) assert(expr)
    //#define ASSERT_RETURN(expr,...) ASSERT(expr,__VA_ARGS__)
#else // NO _DEBUG
    #define ASSERT(expr) {if(!(expr)) {LOGE("assert failed: " #expr "");}}
#endif //_DEBUG
#define __ASSERT_RETURN1(expr) {ASSERT(expr);return;}
#define __ASSERT_RETURN2(expr,result) {ASSERT(expr);if(!(expr)){return (result);}}
#define __ASSERT_RETURN3(expr,result,msg) {ASSERT(expr);if(!(expr)) {LOGE(msg); return (result);}}
#define __ASSERT_RETURN4(expr,result,msg,arg1) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1); return (result);}}
#define __ASSERT_RETURN5(expr,result,msg,arg1,arg2) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1,arg2); return (result);}}
#define __ASSERT_RETURN6(expr,result,msg,arg1,arg2,arg3) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1,arg2,arg3); return (result);}}

#define __ASSERT_RETURN_SELECT(arg1,arg2,arg3,arg4,arg5,arg6,arg7,...)  arg7
#define __ASSERT_RETURN(...) __ASSERT_RETURN_SELECT(__VA_ARGS__,__ASSERT_RETURN6,__ASSERT_RETURN5,__ASSERT_RETURN4,__ASSERT_RETURN3,__ASSERT_RETURN2,__ASSERT_RETURN1 )
#define ASSERT_RETURN(...) __ASSERT_RETURN(__VA_ARGS__)(__VA_ARGS__)

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


#define MAX_CLINETS 500
#define MAX_SENDERS 10

void join_mcast(int fd, struct sockaddr_in *sin);

struct Option
{
    char ip_local[20];
    char ip_remote[20];
    int port;
};