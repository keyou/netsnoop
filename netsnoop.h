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
#include <fstream>
#include <algorithm>

#define TAG "NETSNOOP"

#ifndef BUILD_VERSION
#define BUILD_VERSION 0.0
#endif // !BUILD_VERSION

#define _str1(x) #x
#define _str2(x) _str1(x)
#define VERSION(x) #x _str2(BUILD_VERSION)

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
    Logger(LogLevel level=LLDEBUG):out_(NULL)
    {
        switch (level)
        {
        case LLVERBOSE:out_ = &std::clog;*out_<<"[VER]";break;
        case LLDEBUG:out_ = &std::clog;*out_<<"[DBG]";break;
        case LLWARN:out_ = &std::clog;*out_<<"[WAR]";break;
        case LLERROR:out_ = &std::cerr;*out_<<"[ERR]";break;
        default:out_ = &std::clog;*out_<<"[INF]";
        }

        using namespace std::chrono;

        auto tp = high_resolution_clock::now();
        auto tm = system_clock::to_time_t(tp);
        auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count()%1000;
        char buf[1024]={0};
        // %T is not supported by mingw-w64, why?
        if (std::strftime(buf, sizeof(buf), "%m/%d %H:%M:%S", std::localtime(&tm))) {
            *out_ << "[" << buf <<"."<< ms <<"]";
        }

        *out_<<"["<<std::this_thread::get_id()<<"]";
    }

    // TODO: allow set output stream

    Logger& Tag(const std::string& log)
    {
        *out_<<log;
        return *this;
    }

    std::ostream &GetStream()
    {
        return *out_;
    }

    std::ostream& Print(const char* fmt,...)
    {
        char buf[1024]={0};

        va_list args;
        va_start(args, fmt);
        vsprintf(buf,fmt,args);
        va_end(args);

        *out_<<buf;
        return *out_;
    }
    
    static bool ShouldPrintLog(LogLevel level)
    {
        return GetGlobalLogLevel()<=level;
    }

    /**
     * @brief Set the Global Log Level object,should called before any log be printed.
     * 
     * @param level 
     */
    static void SetGlobalLogLevel(LogLevel level)
    {
        GetGlobalLogLevel() = level;
    }

    static LogLevel& GetGlobalLogLevel()
    {
        static LogLevel global_level = LLERROR;
        return global_level;
    }

    // TODO: support thread name setting
    static std::string& GetThreadName()
    {
        thread_local static std::string thread_name = "NULL";
        return thread_name;
    }

    static void SetThreadName(const std::string& name)
    {
        GetThreadName() = name;
    }

    ~Logger()
    {
        *out_ << std::endl;
    }
private:
    std::ostream* out_;
};
#define __S1(x) #x
#define __S(x) __S1(x)
#define LOG(level) if(Logger::ShouldPrintLog(level))Logger(level).Tag("[" __FILE__ ":" __S(__LINE__) "] ")

#define LOGV LOG(LLVERBOSE).GetStream()
#define LOGD LOG(LLDEBUG).GetStream()
#define LOGI LOG(LLINFO).GetStream()
#define LOGW LOG(LLWARN).GetStream()
#define LOGE LOG(LLERROR).GetStream()

#define LOGVP(...) LOG(LLVERBOSE).Print(__VA_ARGS__)
#define LOGDP(...) LOG(LLDEBUG).Print(__VA_ARGS__)
#define LOGIP(...) LOG(LLINFO).Print(__VA_ARGS__)
#define LOGWP(...) LOG(LLWARN).Print(__VA_ARGS__)
#define LOGEP(...) LOG(LLERROR).Print(__VA_ARGS__)

#ifdef _DEBUG
    #define ASSERT(expr) assert(expr)
    //#define ASSERT_RETURN(expr,...) ASSERT(expr,__VA_ARGS__)
#else // NO _DEBUG
    #define ASSERT(expr) {if(!(expr)) {LOGEP("assert failed: " #expr "");}}
#endif //_DEBUG

#define __ASSERT_RETURN1(expr) {ASSERT(expr);return;}
#define __ASSERT_RETURN2(expr,result) {ASSERT(expr);if(!(expr)){return (result);}}
#define __ASSERT_RETURN3(expr,result,msg) {ASSERT(expr);if(!(expr)) {LOGEP(msg); return (result);}}
#define __ASSERT_RETURN4(expr,result,msg,arg1) {ASSERT(expr);if(!(expr)) {LOGEP(msg,arg1); return (result);}}
#define __ASSERT_RETURN5(expr,result,msg,arg1,arg2) {ASSERT(expr);if(!(expr)) {LOGEP(msg,arg1,arg2); return (result);}}
#define __ASSERT_RETURN6(expr,result,msg,arg1,arg2,arg3) {ASSERT(expr);if(!(expr)) {LOGEP(msg,arg1,arg2,arg3); return (result);}}

#define __ASSERT_RETURN_SELECT(arg1,arg2,arg3,arg4,arg5,arg6,arg7,...)  arg7
#define __ASSERT_RETURN(...) __ASSERT_RETURN_SELECT(__VA_ARGS__,__ASSERT_RETURN6,__ASSERT_RETURN5,__ASSERT_RETURN4,__ASSERT_RETURN3,__ASSERT_RETURN2,__ASSERT_RETURN1 )
#define ASSERT_RETURN(...) __ASSERT_RETURN(__VA_ARGS__)(__VA_ARGS__)

#define EXPORT

#define MAX_CLINETS 500

#define ERR_ILLEGAL_PARAM -7
#define ERR_DEFAULT -1
#define ERR_SOCKET_CLOSED -2
#define ERR_TIMEOUT -3
#define ERR_ILLEGAL_DATA -5
#define ERR_AUTH_ERROR -6
#define ERR_OTHER -99

#define DISALLOW_COPY_AND_ASSIGN(clazz)       \
    clazz(const clazz &) = delete;            \
    clazz &operator=(const clazz &) = delete; 
    // clazz(clazz &&) = delete;                 \
    // clazz &operator=(clazz &&) = delete;


#define MAX_CLINETS 500
#define MAX_SENDERS 10
#define MAX_SEQ (1UL<<16)

struct Option
{
    Option():ip_local{0},ip_remote{0},ip_multicast{0},port{0}{}
    char ip_local[20];
    char ip_remote[20];
    char ip_multicast[20];
    int port;
};

class Tools
{
public:
    static std::string GetDataSum(const std::string& data,size_t length=64)
    {
        std::ostringstream out;
        auto count = std::min(data.length(),length);
        for(size_t i = 0;i<count;i++)
        {
            out<< (isprint(data[i])?data[i]:'.');
        }
        return out.str();
    }
};
