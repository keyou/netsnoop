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

//#include "netsnoop.h"
//#include "udp.h"

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

#define ERR_ILLEGAL_PARAM -7

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

class Sock
{
public:
    static int CreateSocket(int type, int protocol)
    {
        int sockfd;
        LOGV("create socket.\n");
        if ((sockfd = socket(AF_INET, type, protocol)) < 0)
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
        return sockfd;
    }
    static int Bind(int fd_, std::string ip, int port)
    {
        ASSERT(fd_ > 0);
        struct sockaddr_in localaddr;
        memset(&localaddr, 0, sizeof(localaddr));
        localaddr.sin_family = AF_INET;
        localaddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &localaddr.sin_addr) <= 0)
        {
            LOGE("inet_pton local error for %s\n", ip.c_str());
            return ERR_ILLEGAL_PARAM;
        }

        LOGV("bind %s:%d\n", ip.c_str(), port);
        if (bind(fd_, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
        {
            LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        return 0;
    }
    static ssize_t Send(int fd_, const char *buf, size_t size)
    {
        ASSERT(fd_ > 0);
        ssize_t result;
        if ((result = send(fd_, buf, size, 0)) < 0 || result != size)
        {
            LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        LOGV("send: %s\n", buf);
        return result;
    }

    static ssize_t Recv(int fd_, char *buf, size_t size)
    {
        ASSERT(fd_ > 0);
        ssize_t result = 0;
        if ((result = recv(fd_, buf, size, 0)) == -1)
        {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
                LOGE("recv error: %s(errno: %d)\n", strerror(errno), errno);
                return -1;
            }
            LOGE("recv timeout.\n");
#define ERR_TIMEOUT -2
            return ERR_TIMEOUT;
        }
        LOGV("recv: %s\n", buf);
        return result;
    }
    int Initialize()
    {
        int sockfd;
        int opt;

        if (fd_ > 0)
            return fd_;

        sockfd = CreateSocket(type_, protocol_);
        if (sockfd < 0)
            return -1;

        if (InitializeEx(sockfd) < 0)
            return -1;
        fd_ = sockfd;
        return sockfd;
    }
    int Bind(std::string ip, int port)
    {
        local_ip_ = ip;
        local_port_ = port;
        return Bind(fd_, ip, port);
    }
    ssize_t Send(const char *buf, size_t size) const
    {
        return Send(fd_, buf, size);
    }

    ssize_t Recv(char *buf, size_t size) const
    {
        return Recv(fd_, buf, size);
    }

    int GetLocalAddress(std::string& ip,int& port)
    {
        sockaddr_in localaddr;
        socklen_t localaddr_length = sizeof(localaddr);
        if (getsockname(fd_, (sockaddr *)&localaddr, &localaddr_length) < 0)
        {
            LOGE("getsockname error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        char buf[20];
        ip = inet_ntop(AF_INET, &localaddr.sin_addr, buf, sizeof(localaddr));
        port = ntohs(localaddr.sin_port);
        LOGV("Get local socket: %s:%d\n", buf, port);
        return 0;
    }

    virtual int Connect(std::string ip, int port) = 0;
    virtual int Listen(int count) = 0;
    virtual int Accept() = 0;

    int GetFd(){return fd_;}

protected:
    Sock(int type, int protocol)
        : fd_(0),type_(type), protocol_(protocol) {}
    Sock(int type, int protocol, int fd)
        : fd_(fd), type_(type), protocol_(protocol) {}
    virtual int InitializeEx(int fd) const { return fd; };


    int fd_;
    int type_;
    int protocol_;
    std::string local_ip_;
    int local_port_;
    std::string remote_ip_;
    int remote_port_;

    ~Sock()
    {
        if (fd_ > 0)
        {
            LOGV("close socket: fd = %d\n",fd_);
            close(fd_);
            fd_ = -1;
        }
    }

    DISALLOW_COPY_AND_ASSIGN(Sock);
};

#define MAX_CLINETS 500
class Tcp : public Sock
{
public:
    Tcp() : Sock(SOCK_STREAM, IPPROTO_TCP) {}
    Tcp(int fd) : Sock(SOCK_STREAM, IPPROTO_TCP, fd) {}
    int Listen(int count) override
    {
        ASSERT(fd_ > 0);

        if (listen(fd_, count) < 0)
        {
            LOGE("listen error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        return 0;
    }
    int Connect(std::string ip, int port) override
    {
        ASSERT(fd_ > 0);
        struct sockaddr_in remoteaddr;
        memset(&remoteaddr, 0, sizeof(remoteaddr));
        remoteaddr.sin_family = AF_INET;
        remoteaddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &remoteaddr.sin_addr) <= 0)
        {
            LOGE("inet_pton remote error for %s\n", ip.c_str());
            return ERR_ILLEGAL_PARAM;
        }

        LOGV("connect %s %s:%d\n", protocol_ == IPPROTO_TCP ? "tcp" : "udp", ip.c_str(), port);
        if (connect(fd_, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
        {
            LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        remote_ip_ = ip;
        remote_port_ = port;
        return 0;
    }
    int Accept() override
    {
        ASSERT(fd_ > 0);

        int peerfd;
        int result;
        char remote_ip[20] = {0};
        fd_set fds;
        FD_ZERO(&fds);

        struct sockaddr_in peeraddr;
        socklen_t peeraddr_size = sizeof(peeraddr);
        memset(&peeraddr, 0, sizeof(peeraddr));

        if ((peerfd = accept(fd_, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
        {
            LOGE("accept error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
#ifdef _DEBUG
        inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
        LOGV("accept tcp: %s:%d\n", remote_ip, ntohs(peeraddr.sin_port));
#endif
        return peerfd;
    }

private:
    int InitializeEx(int fd) const override
    {
        int opt = 1;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0)
        {
            LOGE("setsockopt TCP_NODELAY error: %s(errno: %d)\n", strerror(errno), errno);
            close(fd);
            return -1;
        }
        return fd;
    }

    DISALLOW_COPY_AND_ASSIGN(Tcp);
};

class Udp : public Sock
{
public:
    Udp() : Sock(SOCK_DGRAM, IPPROTO_UDP) {}
    Udp(int fd) : Sock(SOCK_DGRAM, IPPROTO_UDP, fd) {}
    int Listen(int count) override
    {
        count_ = count;
        return 0;
    }

    int Connect(std::string ip, int port) override
    {
        ASSERT(fd_ > 0);
        struct sockaddr_in remoteaddr;
        socklen_t size = sizeof(remoteaddr);
        memset(&remoteaddr, 0, sizeof(remoteaddr));
        remoteaddr.sin_family = AF_INET;
        remoteaddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &remoteaddr.sin_addr) <= 0)
        {
            LOGE("inet_pton remote error for %s\n", ip.c_str());
            return ERR_ILLEGAL_PARAM;
        }

        if(Connect(fd_,&remoteaddr,size)<0) return -1;

        // char buf;
        // int try_count = 0;
        // while(true)
        // {
        //     if(Send(fd_,"",0) < 0 || Recv(fd_,&buf,1) <0)
        //     {
        //         if( (++try_count) < MAX_TRY_NUM) continue;
        //         LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        //         return -1;
        //     }
        //     break;
        // }
        
        LOGV("Connect udp: %s:%d\n",ip.c_str(),port);
        
        remote_ip_ = ip;
        remote_port_ = port;
        return 0;
    }

    int Accept() override
    {
        ASSERT(fd_>0);
        int result;
        char buf[64] = {0};
        char remote_ip[20] = {0};

        struct sockaddr_in peeraddr;
        socklen_t peeraddr_size = sizeof(peeraddr);
        memset(&peeraddr, 0, sizeof(peeraddr));

        memset(remote_ip, 0, sizeof(remote_ip));

        if ((result = recvfrom(fd_, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
        {
            LOGE("accept error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        inet_ntop(AF_INET, &peeraddr.sin_addr, remote_ip, peeraddr_size);
        LOGV("accept udp from [%s:%d]: %s\n", remote_ip, ntohs(peeraddr.sin_port), buf);

        int peerfd = CreateSocket(type_,protocol_);
        if(Bind(peerfd,local_ip_,local_port_)<0) return -1;
        if(Connect(peerfd,&peeraddr,peeraddr_size)<0) return -1;
        if ((result = Send(peerfd,buf, result)) < 0) return -1;

        return peerfd;
    }

    static const int MAX_TRY_NUM = 3;
private:
    static int Connect(int fd,sockaddr_in* remoteaddr,int size)
    {
        if (connect(fd, (struct sockaddr *)remoteaddr, size) < 0)
        {
            LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }
        return 0;
    }

    int count_;

    DISALLOW_COPY_AND_ASSIGN(Udp);
};

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
    Context() : max_fd(-1), control_fd(-1), data_fd(-1)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
    }

    inline void SetReadFd(int fd)
    {
        FD_SET(fd, &read_fds);
        max_fd = std::max(max_fd, fd);
    }

    inline void SetWriteFd(int fd)
    {
        FD_SET(fd, &write_fds);
        max_fd = std::max(max_fd, fd);
    }

    inline void ClrReadFd(int fd)
    {
        FD_CLR(fd, &read_fds);
    }

    inline void ClrWriteFd(int fd)
    {
        FD_CLR(fd, &write_fds);
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
    Peer(std::shared_ptr<Sock> control_sock, std::shared_ptr<Context> context) 
        : Peer(control_sock,"",context)
    {
    }

    Peer(std::shared_ptr<Sock> control_sock, const std::string cookie, std::shared_ptr<Context> context) 
        : cookie_(cookie), context_(context), cmd_(CMD_NULL),control_sock_(control_sock)
    {
    }

    inline void SetDataSock(std::shared_ptr<Sock> data_sock) { data_sock_ = data_sock; }
    inline int GetDataFd() { return data_sock_?data_sock_->GetFd():-1; }

    int SendCommand()
    {
        if (cmd_ == CMD_NULL)
        {
            if (control_sock_->Send(echo_cmd.c_str(), echo_cmd.length()) == -1)
            {
                LOGE("change mode error.\n");
                return -1;
            }
            context_->SetWriteFd(data_sock_->GetFd());
            context_->ClrReadFd(data_sock_->GetFd());
            cmd_ = CMD_ECHO;
            context_->timeout.tv_nsec = 900 * 1000 * 1000; //100ms
            context_->timeout_callback = [&]() {
                context_->SetWriteFd(data_sock_->GetFd());
            };
        }
        // Stop send control cmd and Start recv control cmd.
        context_->ClrWriteFd(control_sock_->GetFd());
        context_->SetReadFd(control_sock_->GetFd());
        return 0;
    }
    int RecvCommand()
    {
        int result;
        std::string buf(1024,'\0');
        if((result = control_sock_->Recv(&buf[0],buf.length())) <=0 )
        {
            LOGE("Disconnect.\n");
            context_->ClrReadFd(control_sock_->GetFd());
            context_->ClrReadFd(data_sock_->GetFd());
            context_->ClrWriteFd(control_sock_->GetFd());
            context_->ClrWriteFd(data_sock_->GetFd());
            context_->timeout.tv_nsec = 0;
            // auto data = context_->peers.erase(std::remove(context_->peers.begin(),context_->peers.end(), std::make_shared<Peer>(this)),context_->peers.end());
            // ASSERT(data!=context_->peers.end());
            return -1;
        }
        buf.resize(result);
        if(cookie_.empty())
        {
            if(buf.rfind("cookie:",0) != 0)
            {
                LOGE("Bad client.\n");
                // auto data = context_->peers.erase(std::remove(context_->peers.begin(),context_->peers.end(),std::make_shared<Peer>(this)),context_->peers.end());
                // ASSERT(data!=context_->peers.end());
                return -1;
            }
            cookie_ = buf;
            std::string ip;
            int port;

            data_sock_ = std::make_shared<Udp>();
            result = data_sock_->Initialize();
            ASSERT(result >= 0);
            result = control_sock_->GetLocalAddress(ip,port);
            ASSERT(result >= 0);
            data_sock_->Bind(ip,port);
            
            buf = buf.substr(sizeof("cookie:")-1);
            int index = buf.find(':');
            ip = buf.substr(0,index);
            port = atoi(buf.substr(index+1).c_str());
            data_sock_->Connect(ip,port);

            context_->SetWriteFd(control_sock_->GetFd());

            return 0;
        }
        
        LOGV("Recv Command: %s\n",buf.c_str());

        return 0;
    }

    int SendData()
    {
        if (cmd_ == CMD_ECHO)
            return SendEcho();
        if (cmd_ == CMD_RECV)
            return SendEcho();
        if (cmd_ == CMD_SEND)
            return SendEcho();
        LOGE("Peer send error: cmd = %d\n", cmd_);
#define ERR_OTHER -99
        return ERR_OTHER;
    }
    int RecvData()
    {
        if (cmd_ == CMD_ECHO)
            return RecvEcho();
        if (cmd_ == CMD_RECV)
            return RecvEcho();
        if (cmd_ == CMD_SEND)
            return RecvEcho();
        LOGE("Peer recv error: cmd = %d\n", cmd_);
#define ERR_OTHER -99
        return ERR_OTHER;
    }
    int SendEcho()
    {
        context_->SetReadFd(data_sock_->GetFd());
        context_->ClrWriteFd(data_sock_->GetFd());
        const std::string tmp(10, 'a');
        return data_sock_->Send(tmp.c_str(), tmp.length());
    }
    int RecvEcho()
    {
        //context_->SetWriteFd(data_fd_);
        context_->ClrReadFd(data_sock_->GetFd());
        return data_sock_->Recv(buf_, sizeof(buf_));
    }
    
    bool operator==(const Peer& peer)
    {
        return std::addressof(*this) == std::addressof(peer);
    }

    int GetControlFd() { return control_sock_->GetFd(); }
    int GetCmd() { return cmd_; }
    const std::string &GetCookie() { return cookie_; }

private:
    std::shared_ptr<Sock> control_sock_;
    std::shared_ptr<Sock> data_sock_;
    std::string cookie_;
    int cmd_;
    char buf_[1024 * 64];
    std::shared_ptr<Context> context_;

    DISALLOW_COPY_AND_ASSIGN(Peer);
};

class NetSnoopServer
{
public:
    NetSnoopServer(std::shared_ptr<Option> option)
        :option_(option),
        context_(std::make_shared<Context>()),
        listen_tcp_(std::make_shared<Tcp>()),
        listen_udp_(std::make_shared<Udp>())
        {}
    int Run()
    {
        int result;
        timespec timeout = {2, 0};
        timespec *timeout_ptr = NULL;
        fd_set read_fdsets, write_fdsets;
        FD_ZERO(&read_fdsets);
        FD_ZERO(&write_fdsets);

        result = StartListen();
        ASSERT(result >= 0);

        high_resolution_clock::time_point start, end;
        high_resolution_clock::time_point begin = high_resolution_clock::now();
        while (true)
        {
            memcpy(&read_fdsets, &context_->read_fds, sizeof(read_fdsets));
            memcpy(&write_fdsets, &context_->write_fds, sizeof(write_fdsets));
            if (context_->timeout.tv_sec != 0 || context_->timeout.tv_nsec != 0)
            {
                timeout = context_->timeout;
                timeout_ptr = &timeout;
                LOGV("Set timeout: %ld,%ld\n", timeout.tv_sec, timeout.tv_nsec);
            }
            else
            {
                timeout_ptr = NULL;
                LOGV("Clear timeout.\n");
            }

            LOGV("selecting\n");
            result = pselect(context_->max_fd + 1, &read_fdsets, &write_fdsets, NULL, timeout_ptr, NULL);
            LOGV("selected\n");

            for(int i = 0;i<sizeof(fd_set);i++)
            {
                if(FD_ISSET(i,&read_fdsets))
                {
                    std::cout<<"can read: "<<i<<std::endl;
                }
                if(FD_ISSET(i,&write_fdsets))
                {
                    std::cout<<"can write: "<<i<<std::endl;
                }
            }

            if (result < 0)
            {
                // Todo: close socket
                LOGE("select error: %s(errno: %d)\n", strerror(errno), errno);
                return -1;
            }

            if (result == 0 && context_->timeout_callback)
            {
                LOGW("select timeout.\n");
                context_->timeout_callback();
                continue;
            }
            if (FD_ISSET(context_->control_fd, &read_fdsets))
            {
                result = AceeptNewControlConnect();
                ASSERT(result>=0);
            }
            
            for (auto &peer : context_->peers)
            {
                std::cout<<"peer: "<<peer->GetControlFd()<<std::endl;
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
                if (peer->GetDataFd() < 0)
                    continue;
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
private:
    int StartListen()
    {
        int result;
        result = listen_tcp_->Initialize();
        ASSERT(result >= 0);
        result = listen_tcp_->Bind(option_->ip_local,option_->port);
        ASSERT(result >= 0);
        result = listen_tcp_->Listen(MAX_CLINETS);
        ASSERT(result >= 0);

        result = listen_udp_->Initialize();
        ASSERT(result >= 0);
        result = listen_udp_->Bind(option_->ip_local,option_->port);
        ASSERT(result >= 0);
        result = listen_udp_->Listen(MAX_CLINETS);
        ASSERT(result >= 0);
        
        context_->control_fd = listen_tcp_->GetFd();
        context_->SetReadFd(listen_tcp_->GetFd());

        context_->data_fd = listen_udp_->GetFd();
        context_->SetReadFd(listen_udp_->GetFd());
        return 0;
    }
    int AceeptNewControlConnect()
    {
        LOGV("AceeptNewControlConnect.\n");
        int fd;

        if ((fd = listen_tcp_->Accept()) <= 0)
        {
            return -1;
        }
        
        context_->peers.push_back(std::make_shared<Peer>(std::make_shared<Tcp>(fd),context_));
        context_->SetReadFd(fd);

        return fd;
    }

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> listen_tcp_;
    std::shared_ptr<Udp> listen_udp_;
    //std::vector<std::shared_ptr<Peer>> peers_;
    //std::vector<int> half_connect_data_fds_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopServer);
};

int udp_set_timeout(int sockfd, int type, int seconds)
{
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, type, &timeout, sizeof(timeout)) < 0)
    {
        LOGE("setsockopt error:%d,%d\n", type, seconds);
        return -1;
    }
}

class Command
{
public:
    Command(std::shared_ptr<Context> context) : context_(context) {}
    Command(std::string argv, std::shared_ptr<Context> context)
        : argv_(argv), context_(context) {}
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual int Send() { return 0; };
    virtual int Recv() { return 0; };

protected:
    std::string argv_;
    std::shared_ptr<Context> context_;
};

class EchoCommand : public Command
{
public:
    EchoCommand(std::shared_ptr<Context> context)
        : length_(0), count_(0), running_(false), Command(context) {}

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
    int Recv() override
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

private:
    char buf_[1024 * 64];
    int length_;
    ssize_t count_;
    bool running_;
};

class NetSnoopClient
{
public:
    NetSnoopClient(std::shared_ptr<Option> option)
        :option_(option)
    {}
    int Run()
    {
        int result;
        char buf[100] = {0};
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        context_ = std::make_shared<Context>();
        auto context = context_;

        if ((result = Connect()) != 0)
            return result;
        context->SetReadFd(context->control_fd);

        std::shared_ptr<Command> command;

        while (true)
        {
            memcpy(&read_fds, &context->read_fds, sizeof(read_fds));
            memcpy(&write_fds, &context->write_fds, sizeof(write_fds));

            LOGV("selecting\n");
            result = select(context->max_fd + 1, &read_fds, &write_fds, NULL, NULL);
            LOGV("selected\n");

            if (result <= 0)
            {
                LOGE("select error: %d,%d\n", result, errno);
                return -1;
            }
            if (FD_ISSET(context->control_fd, &read_fds))
            {
                if ((result = ParseCommand(command)) > 0)
                {
                    command->Start();
                }
                else
                {
                    LOGE("Parsing cmd error.\n");
                    break;
                }
            }
            if (FD_ISSET(context->control_fd, &write_fds))
            {
            }
            if (FD_ISSET(context->data_fd, &read_fds))
            {
                command->Recv();
            }
            if (FD_ISSET(context->data_fd, &write_fds))
            {
                command->Send();
            }
        }

        return -1;
    }
private:
    int Connect()
    {
        int result;
        tcp_client_ = std::make_shared<Tcp>();
        result = tcp_client_->Initialize();
        ASSERT(result > 0);
        result = tcp_client_->Connect(option_->ip_remote,option_->port);
        ASSERT(result >= 0);

        // char buf[1024 * 64] = {0};
        // ssize_t rlength = 0;
        // srand(high_resolution_clock::now().time_since_epoch().count());
        // std::string cookie = "cookie:" + std::to_string(rand());

        // result = tcp_client_->Send(cookie.c_str(), cookie.length());
        // ASSERT(result >= 0);

        // result = tcp_client_->Recv(buf, sizeof(buf));
        // ASSERT(result >= 0);
        // ASSERT(cookie == buf);

        udp_client_ = std::make_shared<Udp>();
        udp_client_->Initialize();
        udp_client_->Connect(option_->ip_remote,option_->port);

        std::string ip;
        int port;
        result = udp_client_->GetLocalAddress(ip,port);
        ASSERT(result >=0 );

        std::string cookie("cookie:"+ip+":"+std::to_string(port));
        result = tcp_client_->Send(cookie.c_str(),cookie.length());
        ASSERT(result >= 0);

        context_->control_fd = tcp_client_->GetFd();
        context_->data_fd = udp_client_->GetFd();

        return 0;
    }

    int ParseCommand(std::shared_ptr<Command> &command)
    {
        int result;
        std::string cmd(20,'\0');
        LOGV("Client parsing cmd.\n");
        if ((result = tcp_client_->Recv(&cmd[0], cmd.length())) > 0)
        {
    #define ERR_ILLEGAL_DATA -5
            cmd.resize(result);
            
            if (!cmd.rfind("RECV", 0))
                return CMD_RECV;
            if (!cmd.rfind("SEND", 0))
                return CMD_SEND;
            if (!cmd.rfind("ECHO", 0))
            {
                command = std::make_shared<EchoCommand>(context_);
                return CMD_ECHO;
            }
            return ERR_ILLEGAL_DATA;
        }
        return -1;
    }

    std::shared_ptr<Option> option_;
    std::shared_ptr<Context> context_;
    std::shared_ptr<Tcp> tcp_client_;
    std::shared_ptr<Udp> udp_client_;

    DISALLOW_COPY_AND_ASSIGN(NetSnoopClient);
};

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
    auto g_option = std::make_shared<Option>();
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;
    g_option->rate = 2048;
    g_option->buffer_size = 1024 * 8;

    if (argc > 1)
    {
        if (argc > 2)
            g_option->buffer_size = atoi(argv[2]);
        if (!strcmp(argv[1], "-s"))
        {
            LOGV("init_server\n");
            NetSnoopServer(g_option).Run();
        }
        else if (!strcmp(argv[1], "-c"))
        {
            LOGV("init_client\n");
            NetSnoopClient(g_option).Run();
        }
    }
    return 0;
}
