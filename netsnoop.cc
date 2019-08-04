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
#define LOGV(...) //fprintf(stdout, __VA_ARGS__)
#define LOGW(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
//#define LOGV(...)
//#define LOGW(...)
//#define LOGE(...)

#define EXPORT

void join_mcast(int fd, struct sockaddr_in *sin);

struct option
{
    int type;
    char ip_local[20];
    char ip_remote[20];
    int port;
    // Bit Rate
    int rate;
    // Buffer Size
    int buffer_size;
} g_option;


inline ssize_t udp_recv(int sockfd,char* buf,size_t length)
{
    ssize_t result = 0;
    if ((result = recv(sockfd, buf, length, 0)) == -1)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            LOGE("recv error.\n");
            close(sockfd);
            return -1;
        }
        LOGE("recv timeout.\n");
        close(sockfd);
        return -1;
    }
    LOGV("recv: %s\n",buf);
    return result;
}

inline ssize_t udp_send(int sockfd,char* buf,size_t length)
{
    ssize_t result;
    if ((result = send(sockfd, buf, length, 0)) < 0)
    {
        LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    LOGV("send: %s\n", data);
    return result;
}

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

    int max_sd = sockfd;
    int result;
    fd_set read_fds, write_fds;

    LOGV("recv socket.\n");
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
    high_resolution_clock::time_point start, end;
    high_resolution_clock::time_point begin = high_resolution_clock::now();
    while (true)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &read_fds);
        for (i = 0; i < remote_count; i++)
        {
            if (remote_lengths[i] >= 0)
            {
                FD_SET(sockfd, &write_fds);
                LOGV("FD_SET write fd.\n");
                break;
            }
        }
        LOGV("selecting\n");
        result = select(max_sd + 1, &read_fds, &write_fds, NULL, NULL);
        LOGV("selected\n");
        if (result <= 0)
        {
            // Todo: close socket
            return -1;
        }
        if (FD_ISSET(sockfd, &read_fds))
        {
            memset(buf, 0, sizeof(buf));
            memset(remote_ip, 0, sizeof(remote_ip));
            p_remoteaddr = &remoteaddrs[remote_count];
            memset(p_remoteaddr, 0, remote_addr_length);
            if ((rlength = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)p_remoteaddr, &remote_addr_length)) == -1)
            {
                LOGE("recvfrom error.\n");
                continue;
            }
            inet_ntop(AF_INET, &p_remoteaddr->sin_addr, remote_ip, remote_addr_length);
            LOGV("receive from [%s:%d]: %s\n", remote_ip, ntohs(p_remoteaddr->sin_port), buf);
            LOGV("send: %s\n", buf);
            if (sendto(sockfd, buf, rlength, 0, (struct sockaddr *)p_remoteaddr, remote_addr_length) < 0)
            {
                LOGE("sendto error.\n");
                continue;
            }
            if (!strncmp(buf, "cookie:", 7))
            {
                remote_count++;
                current_remote_count++;
                LOGW("connect to [%s:%d]: %s(%d)\n", remote_ip, ntohs(p_remoteaddr->sin_port), buf, remote_count);
                if (current_remote_count == 1)
                    start = high_resolution_clock::now();
            }
        }
        if (FD_ISSET(sockfd, &write_fds))
        {
            for (i = 0; i < remote_count; i++)
            {
                if (remote_lengths[i] == -1)
                    continue;
                p_remoteaddr = &remoteaddrs[i];
                memset(remote_ip, 0, sizeof(remote_ip));
                inet_ntop(AF_INET, &p_remoteaddr->sin_addr, remote_ip, remote_addr_length);
                if (remote_lengths[i] >= 40000)
                {
                    LOGW("stop send: %s:%d\n", remote_ip, ntohs(p_remoteaddr->sin_port));
                    LOGW("Total Rate: %ld/%fms = %f MB/s ; %ld/* = %f pps\n", total_rlength, 1000 * delay, 1.0 * total_rlength / delay / 1024 / 1024, total_count, (total_count) / delay);
                    current_remote_count--;
                    remote_lengths[i] = -1;
                    if (current_remote_count == 0)
                    {
                        total_rlength = 0;
                        total_count = 0;
                    }
                    if ((rlength = sendto(sockfd, "", 0, 0, (struct sockaddr *)p_remoteaddr, remote_addr_length)) < 0)
                    {
                        LOGE("sendto(stop) error.\n");
                    }
                    continue;
                }
                LOGV("send to [%s:%d]: %ldx%ld\n", remote_ip, ntohs(p_remoteaddr->sin_port), data_length, remote_lengths[i] + 1);
                if ((rlength = sendto(sockfd, data.c_str(), data.length(), 0, (struct sockaddr *)p_remoteaddr, remote_addr_length)) < 0)
                {
                    LOGE("sendto error.\n");
                    break;
                }
                else
                {
                    remote_lengths[i]++;
                    total_count++;
                }
                if (rlength != data.length())
                    LOGE("real send length: %ld/%ld\n", rlength, data.length());
            }
            end = high_resolution_clock::now();
            delay = duration<double>(end - start).count();
            total_rlength += rlength > 0 ? rlength : 0;
            LOGV("send: %ldx%ld = %ld (%f pps)\n", rlength, total_count, total_rlength, total_count / delay);
            LOGV("Rate: %ld/%fms = %f MB/s\n", total_rlength, 1000 * delay, 1.0 * total_rlength / delay / 1024 / 1024);
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
    if (send(sockfd,cookie.c_str(),cookie.length(), 0) < 0)
    {
        LOGE("send error: %s(errno: %d)\n", strerror(errno), errno);
        close(sockfd);
        return -1;
    }
    if((result = udp_recv(sockfd,buf,sizeof(buf))) < 0)
    {
        return -1;
    }
    return result;
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
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        LOGE("setsockopt error:SO_RCVTIMEO\n");
        close(sockfd);
        return -1;
    }

    if((result = connect_server(sockfd))<0) return result;

    char buf[1024 * 64] = {0};
    ssize_t slength = 0;
    ssize_t rlength = 0;
    ssize_t total_rlength = 0;
    ssize_t count = 0;
    ssize_t index = 0;
    bool begin_delay_test = false;
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
        close(sockfd);
        return -1;
    }
    delay = duration<double>(end - start).count();
    LOGW("Total Rate: %ld/%fms = %f MB/s ; %ld/* = %f pps\n", total_rlength, 1000 * delay, 1.0 * total_rlength / delay / 1024 / 1024, count, count / delay);
    if(rlength != 0) return -1;
    
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
