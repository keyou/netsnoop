#include <iostream>
#include <functional>
#include <thread>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

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
            NetSnoopServer server(g_option);
            auto t = std::thread([&server](){
                LOGV("init_server\n");
                server.Run();
            });
            t.detach();
            std::string data;
            while(true)
            {
                std::cout<<"Input Action:";
                std::getline(std::cin,data);
                server.SendCommand(data);
            }
        }
        else if (!strcmp(argv[1], "-c"))
        {
            NetSnoopClient client(g_option);
            auto t = std::thread([&client](){
                LOGV("init_client\n");
                client.Run();
            });
            t.join();
        }
    }


    return 0;
}

// void join_mcast(int fd, struct sockaddr_in *sin)
// {
//     u_long inaddr;
//     struct ip_mreq mreq;

//     inaddr = sin->sin_addr.s_addr;
//     if (IN_MULTICAST(ntohl(inaddr)) == 0)
//         return;

//     mreq.imr_multiaddr.s_addr = inaddr;
//     mreq.imr_interface.s_addr = htonl(INADDR_ANY); /* need way to change */
//     if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
//     {
//         LOGE("IP_ADD_MEMBERSHIP error");
//     }

//     LOGV("multicast group joined\n");
// }