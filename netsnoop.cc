#include <iostream>
#include <functional>
#include <thread>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

int main(int argc, char *argv[])
{
    std::cout << "hello,netsnoop!" << std::endl;
    auto g_option = std::make_shared<Option>();
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;

    if(argc>2)
    {
        strncpy(g_option->ip_remote, argv[2], sizeof(g_option->ip_remote));
        strncpy(g_option->ip_local, argv[2], sizeof(g_option->ip_local));
    }

    if (argc > 1)
    {
        if (!strcmp(argv[1], "-s"))
        {
            NetSnoopServer server(g_option);
            auto t = std::thread([&server]() {
                LOGV("init_server\n");
                server.Run();
            });
            t.detach();
            std::string cmd;
            while (true)
            {
                std::cout << "Input Action:";
                std::getline(std::cin, cmd);
                if(cmd.length()<4) continue;
                auto command = CommandFactory::New(cmd);
                if (command)
                {
                    command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                        std::clog << "command finish: " << oldcommand->cmd<< " >> "<<(stat?stat->ToString():"NULL") << std::endl;
                    });
                    server.PushCommand(command);
                }
            }
        }
        else if (!strcmp(argv[1], "-c"))
        {
            NetSnoopClient client(g_option);
            auto t = std::thread([&client]() {
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