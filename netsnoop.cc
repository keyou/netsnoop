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

void join_mcast(int fd, struct sockaddr_in *sin);

struct option
{
    char ip_local[20];
    char ip_remote[20];
    int port;
    // Bit Rate
    int rate;
    // Buffer Size
    int buffer_size;
} g_option;


inline ssize_t sock_recv(int sockfd,char* buf,size_t size)
{
    ssize_t result = 0;
    if ((result = recv(sockfd, buf, size, 0)) == -1)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            LOGE("recv error.\n");
            return -1;
        }
        LOGE("recv timeout.\n");
        #define ERR_TIMEOUT -2
        return ERR_TIMEOUT;
    }
    // TODO: distinguish tcp and udp
    if(result == 0) return -1;
    LOGV("recv: %s\n",buf);
    return result;
}

inline ssize_t sock_send(int sockfd,const char* buf,size_t size)
{
    ssize_t result;
    if ((result = send(sockfd, buf, size, 0)) < 0 || result != size)
    {
        LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    
    LOGV("send: %s\n", buf);
    return result;
}


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
            context_->timeout.tv_sec = 1;
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


int udp_listen()
{
    int sockfd;
    struct sockaddr_in localaddr;
    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(g_option.port);

    if (inet_pton(AF_INET, g_option.ip_local, &localaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton local error for %s\n", g_option.ip_local);
        return -1;
    }

    LOGV("create udp listen socket.\n");
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGE("setsockopt SO_REUSEADDR error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    LOGV("bind udp %s:%d\n", g_option.ip_local, g_option.port);
    if (bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int tcp_create()
{
    int sockfd;

    LOGV("create tcp socket.\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGE("setsockopt SO_REUSEADDR error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    opt = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0)
    {
        LOGE("setsockopt TCP_NODELAY error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int tcp_listen()
{
    int sockfd;
    struct sockaddr_in localaddr;
    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(g_option.port);

    if (inet_pton(AF_INET, g_option.ip_local, &localaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton local error for %s\n", g_option.ip_local);
        return -1;
    }

    if((sockfd = tcp_create())<0)
    {
        return -1;
    }

    LOGV("bind %s:%d\n", g_option.ip_local, g_option.port);
    if (bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    #define MAX_CLINETS 500
    if(listen(sockfd,MAX_CLINETS) < 0)
    {
        LOGE("listen error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    return sockfd;
}


int tcp_accept(const std::shared_ptr<Context>& context,std::shared_ptr<Peer>& peer)
{
    int result,fd;
    char buf[1024 * 64] = {0};
    char remote_ip[20] = {0};
    fd_set fds;
    FD_ZERO(&fds);

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr,0,sizeof(peeraddr));

    if ((fd = accept(context->control_fd,(struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        LOGE("accept error.\n");
        return -1;
    }


    #define SOCKET_READ_TIMEOUT_SEC 1
    struct timeval timeout;
    timeout.tv_sec = SOCKET_READ_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    FD_SET(fd,&fds);
    result = select(fd + 1, &fds, NULL, NULL, &timeout);
    if(result <= 0)
    {
        LOGE("Read cookie error: %s(errno: %d)\n", strerror(errno), errno);
        close(fd);
        return -1;
    }
    
    if((result = sock_recv(fd,buf,sizeof(buf))) == -1) 
    {
        LOGE("tcp recv error");
        close(fd);
        return -1;
    }
    if (strncmp(buf, "cookie:", 7))
    {
        LOGE("Cookie format error.\n");
        #define ERR_ILLEGAL_COOKIE -3
        return ERR_ILLEGAL_COOKIE;
    }

    inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
    LOGV("accept tcp from [%s:%d]: %s\n", remote_ip, ntohs(peeraddr.sin_port), buf);

    if ((result = sock_send(fd, buf, result)) < 0)
    {
        close(fd);
        return -1;
    }
    
    peer = std::make_shared<Peer>(fd,buf,context);

    return fd;
}

int udp_server_parse()
{
    return 0;
}

int udp_accept(std::shared_ptr<Context> context,std::shared_ptr<Peer>& peer)
{
    int result;
    char buf[1024 * 64] = {0};
    char remote_ip[20] = {0};

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr,0,sizeof(peeraddr));

    memset(remote_ip, 0, sizeof(remote_ip));

    if ((result = recvfrom(context->data_fd, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
    {
        LOGE("recvfrom error.\n");
        return -1;
    }

#define ERR_ILLEGAL_COOKIE -3
    if (strncmp(buf, "cookie:", 7))
    {
        LOGE("Cookie format error.\n");
        return ERR_ILLEGAL_COOKIE;
    }
    
    inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
    LOGV("accept udp from [%s:%d]: %s\n", remote_ip, ntohs(peeraddr.sin_port), buf);
    
    if (connect(context->data_fd, (struct sockaddr *)&peeraddr, peeraddr_size) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    if ((result = sock_send(context->data_fd, buf, result)) < 0)
    {
        return -1;
    }

    for(auto& p:context->peers)
    {
        if(p->GetCookie() == buf)
        {
            peer = p;
            break;
        }
    }

    if ((result = udp_listen()) < 0)
    {
        LOGE("create data socket error: %s(errno: %d)\n", strerror(errno), errno);
        exit(-1);
        return -1;
    }
    
    return result;
}


int StartListen()
{
    int sockfd;
    struct sockaddr_in localaddr;
    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(g_option.port);

    if ((sockfd = tcp_listen()) == -1)
    {
        LOGE("listen local error for %s\n", g_option.ip_local);
        return -1;
    }
    return sockfd;
}

extern "C" EXPORT int init_server()
{
    int result;
    auto context = std::make_shared<Context>();
    
    if ((result = StartListen()) <= 0)
    {
        return -1;
    }
    context->control_fd = result;
    context->SetReadFd(result);

    if((result = udp_listen()) <=0 )
    {
        return -1;    
    }
    context->data_fd = result;
    context->SetReadFd(result);

    timespec timeout;
    timespec* timeout_ptr = NULL;
    fd_set read_fdsets, write_fdsets;
    FD_ZERO(&read_fdsets);
    FD_ZERO(&write_fdsets);

    high_resolution_clock::time_point start, end;
    high_resolution_clock::time_point begin = high_resolution_clock::now();
    while (true)
    {
        memcpy(&read_fdsets,&context->read_fds,sizeof(read_fdsets));
        memcpy(&write_fdsets,&context->write_fds,sizeof(write_fdsets));
        if(context->timeout.tv_sec!=0||context->timeout.tv_nsec!=0)
        {
            timeout = context->timeout;
            timeout_ptr = &timeout;
        }
        else
        {
            timeout_ptr = NULL;
        }
        
        LOGV("selecting\n");
        result = pselect(context->max_fd+1, &read_fdsets, &write_fdsets, NULL,timeout_ptr,NULL);
        LOGV("selected\n");
        
        if (result < 0)
        {
            // Todo: close socket
            LOGE("select error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        if(result == 0 && context->timeout_callback)
        {
            LOGW("select timeout.\n");
            context->timeout_callback();
            continue;
        }
        if (FD_ISSET(context->control_fd, &read_fdsets))
        {
            std::shared_ptr<Peer> peer;
            result = tcp_accept(context,peer);
            if(result>0)
            {
                context->peers.push_back(peer);
            }
        }
        if(FD_ISSET(context->data_fd,&read_fdsets))
        {
            std::shared_ptr<Peer> peer;
            if((result = udp_accept(context,peer)) >0)
            {
                LOGV("Accept success.\n");
                FD_CLR(context->data_fd,&read_fdsets);
                peer->SetDataFd(context->data_fd);
                context->SetWriteFd(peer->GetControlFd());
                context->data_fd = result;
                context->SetReadFd(context->data_fd);
            }
        }
        for(auto& peer : context->peers)
        {
            if (FD_ISSET(peer->GetControlFd(), &write_fdsets))
            {
                LOGV("Sending Command.\n");
                peer->SendCommand();
            }
            if (FD_ISSET(peer->GetControlFd(), &read_fdsets))
            {
                LOGV("Recving Command.\n");
                peer->RecvCommand();
            }
            if(peer->GetDataFd()<0) continue;
            if (FD_ISSET(peer->GetDataFd(), &write_fdsets))
            {
                LOGV("Sending Data.\n");
                peer->SendData();
            }
            if (FD_ISSET(peer->GetDataFd(), &read_fdsets))
            {
                LOGV("Recving Data.\n");
                peer->RecvData();
            }
        }
    }

    return 0;
}


int tcp_connect()
{
    int sockfd;
    struct sockaddr_in remoteaddr;

    memset(&remoteaddr, 0, sizeof(remoteaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(g_option.port);

    if (inet_pton(AF_INET, g_option.ip_remote, &remoteaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton remote error for %s\n", g_option.ip_remote);
        return -1;
    }

    sockfd = tcp_create();
    ASSERT(sockfd>0);

    LOGV("tcp connect %s:%d\n", g_option.ip_remote, g_option.port);
    if (connect(sockfd, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
    {
        LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int connect_server(std::shared_ptr<Context> context)
{
    int result;
    int sockfd;
    sockfd = tcp_connect();
    ASSERT(sockfd>0);
    
    char buf[1024*64] = {0};
    ssize_t rlength = 0;
    srand(high_resolution_clock::now().time_since_epoch().count());
    std::string cookie = "cookie:" + std::to_string(rand());
    
    context->control_fd = sockfd;
    
    result = sock_send(sockfd,cookie.c_str(),cookie.length());
    ASSERT(result>=0);

    result = sock_recv(sockfd,buf,sizeof(buf));
    ASSERT(result>=0);
    ASSERT(cookie == buf);
    
    struct sockaddr_in remoteaddr, localaddr;

    memset(&remoteaddr, 0, sizeof(remoteaddr));
    memset(&localaddr, 0, sizeof(localaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(g_option.port);

    if (inet_pton(AF_INET, g_option.ip_remote, &remoteaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton remote error for %s\n", g_option.ip_remote);
        return -1;
    }
    if (inet_pton(AF_INET, g_option.ip_local, &localaddr.sin_addr) <= 0)
    {
        LOGE("inet_pton local error for %s\n", g_option.ip_local);
        return -1;
    }

    LOGV("create udp socket.\n");
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    LOGV("udp connect %s:%d\n", g_option.ip_remote, g_option.port);
    if (connect(sockfd, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    context->data_fd = sockfd;

    result = sock_send(sockfd,cookie.c_str(),cookie.length());
    ASSERT(result>=0);

    result = sock_recv(sockfd,buf,sizeof(buf));
    ASSERT(result>=0);
    ASSERT(cookie == buf);

    // socklen_t localaddr_length = sizeof(localaddr);
    // if (getsockname(sockfd, (sockaddr *)&localaddr, &localaddr_length) < 0)
    // {
    //     LOGE("getsockname error: %s(errno: %d)\n", strerror(errno), errno);
    //     close(sockfd);
    //     return -1;
    // }

    // LOGV("local socket: %s:%d\n", inet_ntop(AF_INET, &localaddr.sin_addr, g_option.ip_local, sizeof(localaddr)), ntohs(localaddr.sin_port));
    // if (udp_set_timeout(sockfd, SO_RCVTIMEO,2) < 0)
    // {
    //     LOGE("setsockopt error:SO_RCVTIMEO\n");
    //     close(sockfd);
    //     return -1;
    // }

    return 0;
}

int udp_set_timeout(int sockfd,int type,int seconds)
{
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, type, &timeout, sizeof(timeout)) < 0)
    {
        LOGE("setsockopt error:%d,%d\n",type,seconds);
        return -1;
    }
}

class Command
{
public:
    Command(std::shared_ptr<Context> context):context_(context){}
    Command(std::string argv,std::shared_ptr<Context> context)
        :argv_(argv),context_(context){}
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual int Send() {return 0;};
    virtual int Recv() {return 0;};
protected:
    std::string argv_;
    std::shared_ptr<Context> context_;
};

class EchoCommand: public Command
{
public:
    EchoCommand(std::shared_ptr<Context> context)
        :length_(0),count_(0),running_(false),Command(context){}

    void Start() override
    {
        running_ = true;
        LOGV("Echo Start.\n");
        context_->SetReadFd(context_->data_fd);
    }
    void Stop() override
    {
        running_ = false;
        LOGV("Echo Stop.\n");
        context_->ClrReadFd(context_->data_fd);
    }
    int Send() override
    {
        LOGV("Echo Send.\n");
        int result;
        if(count_<= 0) return 0;
        if ((result = sock_send(context_->data_fd, buf_, length_)) < 0)
        {
            return -1;
        }
        count_--;
        if(count_<=0) 
        {
            context_->ClrWriteFd(context_->data_fd);
            if(running_) context_->SetReadFd(context_->data_fd);
        }
        return 0;
    }
    int Recv() override
    {
        LOGV("Echo Recv.\n");
        int result;
        if((result = sock_recv(context_->data_fd,buf_,sizeof(buf_))) < 0 )
        {
            return -1;
        }
        length_ = result;
        context_->SetWriteFd(context_->data_fd);
        context_->ClrReadFd(context_->data_fd);
        count_++;
        return 0;
    }
    
private:
    char buf_[1024*64];
    int length_;
    ssize_t count_;
    bool running_;
};

int tcp_parse_cmd(std::shared_ptr<Context> context,std::shared_ptr<Command>& command)
{
    int result;
    char data[100] = {0};
    char cmd[100] = {0};
    LOGV("Client parsing cmd.\n");
    if((result = sock_recv(context->control_fd,data,sizeof(data))) > 0)
    {
        #define ERR_ILLEGAL_DATA -5
        if(result < 4 || result >= 100) return ERR_ILLEGAL_DATA;
        sscanf(data,"%s",cmd);
        if(!strcmp("RECV",cmd)) return CMD_RECV;
        if(!strcmp("SEND",cmd)) return CMD_SEND;
        if(!strcmp("ECHO",cmd)) 
        {
            command = std::make_shared<EchoCommand>(context);
            return CMD_ECHO;
        }
        return ERR_ILLEGAL_DATA;
    }
    return -1;
}


int udp_cmd_recv(int sockfd)
{
    char buf[1024 * 64] = {0};
    ssize_t rlength = 0;
    ssize_t total_rlength = 0;
    ssize_t count = 0;
    double delay = 0;
    double total_delay = 0;
    high_resolution_clock::time_point start,end;
    
    start = high_resolution_clock::now();
    while ((rlength = sock_recv(sockfd, buf, sizeof(buf))) > 0)
    {
        total_rlength += rlength;
        count++;
    }
    end = high_resolution_clock::now();
    if(rlength < 0) 
    {
        return -1;
    }
    delay = duration<double>(end - start).count();
    LOGW("Total Rate: %ld/%fms = %f MB/s ; %ld/* = %f pps\n", total_rlength, 1000 * delay, 1.0 * total_rlength / delay / 1024 / 1024, count, count / delay);
    return 0;
}

int udp_cmd_send(int sockfd,char* argv)
{
    int size,count;
    if(sscanf(argv,"%d %d",&size,&count) != 2)
    {
        LOGE("Illegal args\n");
        return -2;
    }
    if(size <= 0 || size > 64*1024 || count <= 0)
    {
        LOGE("Args out of range: %d %d\n",size,count);
        return -2;
    }
    std::string tmp(size,'\0');
    const char *buf = tmp.c_str();
    ssize_t rlength = 0;
    do
    {
        if ((rlength = sock_send(sockfd, buf, size)) < 0)
        {
            return -1;
        }
        count --;
    } while (rlength == size && count>0);
    return 0;
}


extern "C" EXPORT int init_client()
{
    int result;
    char buf[100] = {0};
    fd_set read_fds,write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    auto context = std::make_shared<Context>();
    
    if((result = connect_server(context)) != 0) return result;
    context->SetReadFd(context->control_fd);

    std::shared_ptr<Command> command;

    while(true)
    {
        memcpy(&read_fds,&context->read_fds,sizeof(read_fds));
        memcpy(&write_fds,&context->write_fds,sizeof(write_fds));

        result = select(context->max_fd+1,&read_fds,&write_fds,NULL,NULL);
        if(result<=0)
        {
            LOGE("select error: %d,%d\n",result,errno);
            return -1;
        }
        if(FD_ISSET(context->control_fd,&read_fds))
        {
            if((result = tcp_parse_cmd(context,command)) > 0)
            {
                command->Start();
            }
            else
            {
                LOGE("Parsing cmd error.\n");
                break;
            }
        }
        if(FD_ISSET(context->control_fd,&write_fds))
        {

        }
        if(FD_ISSET(context->data_fd,&read_fds))
        {
            command->Recv();
        }
        if(FD_ISSET(context->data_fd,&write_fds))
        {
            command->Send();
        }
    }


    return -1;
}


void join_mcast(int fd, struct sockaddr_in *sin)
{
    u_long inaddr;
    struct ip_mreq mreq;

    inaddr = sin->sin_addr.s_addr;
    if (IN_MULTICAST(ntohl(inaddr)) == 0)
        return;

    mreq.imr_multiaddr.s_addr = inaddr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); /* need way to change */
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
    {
        LOGE("IP_ADD_MEMBERSHIP error");
    }

    LOGV("multicast group joined\n");
}

int main(int argc, char *argv[])
{
    std::cout << "hello,world!" << std::endl;
    strncpy(g_option.ip_remote, "127.0.0.1", sizeof(g_option.ip_remote));
    strncpy(g_option.ip_local, "0.0.0.0", sizeof(g_option.ip_local));
    g_option.port = 4000;
    g_option.rate = 2048;
    g_option.buffer_size = 1024 * 8;

    if (argc > 1)
    {
        if (argc > 2)
            g_option.buffer_size = atoi(argv[2]);
        if (!strcmp(argv[1], "-s"))
        {
            LOGV("init_server\n");
            init_server();
        }
        else if (!strcmp(argv[1], "-c"))
        {
            LOGV("init_client\n");
            init_client();
        }
    }
    return 0;
}
