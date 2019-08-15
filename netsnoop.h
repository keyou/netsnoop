#pragma once

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>
#include <thread>

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
#ifdef _DEBUG
    #define LOGV(...) {std::cout<<"["<<std::this_thread::get_id()<<"]";fprintf(stdout, "[LOGV] " __VA_ARGS__);}
    #define LOGW(...) {std::cout<<"["<<std::this_thread::get_id()<<"]";fprintf(stdout, "[WARN] " __VA_ARGS__);}
    #define LOGE(...) {std::cerr<<"["<<std::this_thread::get_id()<<"]";fprintf(stderr, "[ERRO] " __VA_ARGS__);}
#else // NO _DEBUG
    #define LOGV(...) //{std::cout<<"["<<std::this_thread::get_id()<<"]";fprintf(stdout, "[LOGV] " __VA_ARGS__);}
    #define LOGW(...) {std::cout<<"["<<std::this_thread::get_id()<<"]";fprintf(stdout, "[WARN] " __VA_ARGS__);}
    #define LOGE(...) {std::cerr<<"["<<std::this_thread::get_id()<<"]";fprintf(stderr, "[ERRO] " __VA_ARGS__);}
#endif // _DEBUG

#ifdef _DEBUG
    #define ASSERT(expr,...) assert(expr)
    #define ASSERT_RETURN(expr,...) ASSERT(expr,__VA_ARGS__)
#else // NO _DEBUG
    #define ASSERT(expr) {if(!(expr)) {LOGE("assert failed: " #expr "\n");}}
    #define __ASSERT_RETURN1(expr) {ASSERT(expr);return;}
    #define __ASSERT_RETURN2(expr,result) {ASSERT(expr);if(!(expr)){return (result);}}
    #define __ASSERT_RETURN3(expr,result,msg) {ASSERT(expr);if(!(expr)) {LOGE(msg); return (result);}}
    #define __ASSERT_RETURN4(expr,result,msg,arg1) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1); return (result);}}
    #define __ASSERT_RETURN5(expr,result,msg,arg1,arg2) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1,arg2); return (result);}}
    #define __ASSERT_RETURN6(expr,result,msg,arg1,arg2,arg3) {ASSERT(expr);if(!(expr)) {LOGE(msg,arg1,arg2,arg3); return (result);}}

    #define __ASSERT_RETURN_SELECT(arg1,arg2,arg3,arg4,arg5,arg6,arg7,...)  arg7
    #define __ASSERT_RETURN(...) __ASSERT_RETURN_SELECT(__VA_ARGS__,__ASSERT_RETURN6,__ASSERT_RETURN5,__ASSERT_RETURN4,__ASSERT_RETURN3,__ASSERT_RETURN2,__ASSERT_RETURN1 )
    #define ASSERT_RETURN(...) __ASSERT_RETURN(__VA_ARGS__)(__VA_ARGS__)
#endif //_DEBUG

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


void join_mcast(int fd, struct sockaddr_in *sin);

struct Option
{
    char ip_local[20];
    char ip_remote[20];
    int port;
};