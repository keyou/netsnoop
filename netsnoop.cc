#include <iostream>
#include <netinet/in.h>
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

#include "netsnoop.h"
#include "udp.h"

using namespace std::chrono;

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


inline ssize_t udp_recv(int sockfd,char* buf,size_t size)
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
        return -2;
    }
    LOGV("recv: %s\n",buf);
    return result;
}

inline ssize_t udp_send(int sockfd,const char* buf,size_t size)
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

    LOGV("create new listen socket.\n");
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

    LOGV("bind %s:%d\n", g_option.ip_local, g_option.port);
    if (bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        LOGE("bind error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int udp_server_parse()
{
    return 0;
}

int udp_accept(int sockfd)
{
    int result;
    char buf[1024 * 64] = {0};
    char remote_ip[20] = {0};

    struct sockaddr_in peeraddr;
    socklen_t peeraddr_size = sizeof(peeraddr);
    memset(&peeraddr,0,sizeof(peeraddr));

    memset(remote_ip, 0, sizeof(remote_ip));

    if ((result = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &peeraddr_size)) == -1)
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
    LOGV("accept from [%s:%d]: %s\n", remote_ip, ntohs(peeraddr.sin_port), buf);
    
    if (connect(sockfd, (struct sockaddr *)&peeraddr, peeraddr_size) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    if ((result = udp_send(sockfd, buf, result)) < 0)
    {
        close(sockfd);
        return -1;
    }

    if ((result = udp_listen()) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    
    return result;
}

enum CommandType : char
{
    CMD_NULL = 0,
    CMD_RECV = 1,
    CMD_SEND = 2,
    CMD_ECHO = 3
};

typedef int (*udp_echo_send)();
class Peer
{
public:
    Peer(int fd,int cmd):
        fd_(fd),cmd_(cmd)
    {
        if(cmd_ == CMD_ECHO) {need_write = true;}
    }
    Peer(const Peer& peer)
    {
        fd_ = peer.fd_;
        cmd_ = peer.cmd_;
        need_read = peer.need_read;
        need_write = peer.need_write;
        LOGV("copying\n");
    }

    int send()
    {
        if(cmd_ == CMD_ECHO) return echo_send();
        if(cmd_ == CMD_RECV) return echo_send();
        if(cmd_ == CMD_SEND) return echo_send();
        LOGE("Peer send error: cmd = %d\n",cmd_);
        #define ERR_OTHER -99
        return ERR_OTHER;
    }
    int recv()
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
        need_write = false;
        need_read = true;
        const std::string tmp(10,'a');
        return udp_send(fd_,tmp.c_str(),tmp.length());
    }
    int echo_recv()
    {
        need_write = true;
        need_read = false;
        return udp_recv(fd_,buf_,sizeof(buf_));
    }
    int GetFd(){return fd_;}
    int GetCmd(){return cmd_;}
    bool need_read;
    bool need_write;
private:
    int fd_;
    int cmd_;
    char buf_[1024*64];
};

extern "C" EXPORT int init_server()
{
    int sockfd;
    struct sockaddr_in localaddr;
    struct sockaddr_in *p_remoteaddr;
    struct sockaddr_in remoteaddrs[100];
    ssize_t remote_lengths[100] = {0};
    int remote_count = 0;

    memset(&localaddr, 0, sizeof(localaddr));
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(g_option.port);

    if ((sockfd = udp_listen()) == -1)
    {
        LOGE("listen local error for %s\n", g_option.ip_local);
        return -1;
    }

    int max_fd = sockfd;
    int result;
    fd_set read_fdsets, write_fdsets;

    int i;
    char buf[1024 * 64] = {0};
    int data_length = g_option.buffer_size;
    std::string data(data_length, 'a');
    char remote_ip[20] = {0};
    ssize_t rlength = 0;
    ssize_t count = 0;
    ssize_t total_rlength = 0;
    ssize_t total_count = 0;
    int current_remote_count = 0;
    double delay = 0;
    bool delay_test_begin = false;
    socklen_t remote_addr_length = sizeof(struct sockaddr_in);

    std::vector<Peer> peers;

    const std::string echo_cmd("ECHO");
    const std::string recv_cmd("RECV");
    const std::string send_cmd("SEND 1024 10");

    high_resolution_clock::time_point start, end;
    high_resolution_clock::time_point begin = high_resolution_clock::now();
    while (count++ < 10)
    {
        FD_ZERO(&read_fdsets);
        FD_ZERO(&write_fdsets);
        FD_SET(sockfd, &read_fdsets);
        for(auto peer:peers)
        {
            LOGV("peer: w = %d; r = %d;\n",peer.need_write,peer.need_read);
            if (peer.need_write) FD_SET(peer.GetFd(),&write_fdsets);
            if (peer.need_read) FD_SET(peer.GetFd(),&read_fdsets);
        }
        
        LOGV("selecting\n");
        result = select(max_fd + 1, &read_fdsets, &write_fdsets, NULL, NULL);
        LOGV("selected\n");
        if (result <= 0)
        {
            // Todo: close socket
            return -1;
        }
        if (FD_ISSET(sockfd, &read_fdsets))
        {
            int listenfd = udp_accept(sockfd);
            if(udp_send(sockfd,echo_cmd.c_str(),echo_cmd.length()) == -1)
            {
                close(sockfd);
                max_fd = listenfd;
            }
            else
            {
                peers.push_back(Peer(sockfd,CMD_ECHO));
                max_fd = std::max(max_fd,listenfd);
            }
            sockfd = listenfd;
            continue;
        }
        for(auto peer : peers)
        {
            if (FD_ISSET(peer.GetFd(), &write_fdsets))
            {
                if(peer.send() == -1)
                {
                    LOGE("echo send error.\n");
                }
            }
            if (FD_ISSET(peer.GetFd(), &read_fdsets))
            {
                if(peer.recv() == -1)
                {
                    LOGE("echo recv error.\n");
                }
            }
        }
    }

    LOGV("close socket.\n");
    close(sockfd);
    return 0;
}


int connect_server(int sockfd)
{
    char buf[1024*64] = {0};
    ssize_t rlength = 0;
    int result;
    srand(high_resolution_clock::now().time_since_epoch().count());
    std::string cookie = "cookie:" + std::to_string(rand());
    for(int retry_count=3;retry_count>0;retry_count--)
    {
        if (send(sockfd,cookie.c_str(),cookie.size(), 0) == -1)
        {
            LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
            close(sockfd);
            return -1;
        }
        if((result = udp_recv(sockfd,buf,sizeof(buf))) == -1)
        {
            close(sockfd);
            return -1;
        }
        if(result>=0) break; 
    }
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

int udp_parse_cmd(int sockfd,char* buf,int size)
{
    int result;
    char data[100] = {0};
    char cmd[100] = {0};
    LOGV("client parse cmd.\n");
    while((result = udp_recv(sockfd,data,sizeof(data))) != -1)
    {
        if(result == -2) continue;
        if(result < 4 || result >= 100) continue;
        sscanf(data,"%s",cmd);
        strncpy(buf,data+strlen(cmd),size);
        if(!strcmp("RECV",cmd)) return CMD_RECV;
        if(!strcmp("SEND",cmd)) return CMD_SEND;
        if(!strcmp("ECHO",cmd)) return CMD_ECHO;
        memset(data,0,sizeof(data));
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
    while ((rlength = udp_recv(sockfd, buf, sizeof(buf))) > 0)
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
        if ((rlength = udp_send(sockfd, buf, size)) < 0)
        {
            return -1;
        }
        count --;
    } while (rlength == size && count>0);
    return 0;
}

int udp_cmd_echo(int sockfd)
{
    char buf[1024*64] = {0};
    ssize_t rlength = 0;
    do
    {
        if((rlength = udp_recv(sockfd,buf,sizeof(buf))) < 0 )
        {
            close(sockfd);
            return -1;
        }
        if ((rlength = udp_send(sockfd, buf, rlength)) < 0)
        {
            close(sockfd);
            return -1;
        }
    } while (rlength > 0);
    return 0; 
}

extern "C" EXPORT int init_client()
{
    int result;
    int sockfd;
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

    LOGV("create socket.\n");
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        LOGE("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    LOGV("connect %s:%d\n", g_option.ip_remote, g_option.port);
    if (connect(sockfd, (struct sockaddr *)&remoteaddr, sizeof(remoteaddr)) < 0)
    {
        LOGE("connect error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    socklen_t localaddr_length = sizeof(localaddr);
    if (getsockname(sockfd, (sockaddr *)&localaddr, &localaddr_length) < 0)
    {
        LOGE("getsockname error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }

    LOGV("local socket: %s:%d\n", inet_ntop(AF_INET, &localaddr.sin_addr, g_option.ip_local, sizeof(localaddr)), ntohs(localaddr.sin_port));
    if (udp_set_timeout(sockfd, SO_RCVTIMEO,2) < 0)
    {
        LOGE("setsockopt error:SO_RCVTIMEO\n");
        close(sockfd);
        return -1;
    }

    if((result = connect_server(sockfd)) != 0) return result;

    char buf[100] = {0};
    while((result = udp_parse_cmd(sockfd,buf,sizeof(buf)-1)) > 0)
    {
        switch (result)
        {
        case CMD_RECV:
            if( (result = udp_cmd_recv(sockfd)) == -1) return result;    
            break;
        case CMD_SEND:
            if( (result = udp_cmd_send(sockfd,buf)) == -1) return result; 
            break;
        case CMD_ECHO:
            if( (result = udp_cmd_echo(sockfd)) == -1) return result;
            break;
        default:
            LOGW("NOT REACHABLE.\n");
            exit(-1);
        }
        memset(buf,0,sizeof(buf));
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
