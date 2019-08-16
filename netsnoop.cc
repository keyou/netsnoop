#include <iostream>
#include <functional>
#include <thread>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

void StartClient();
void StartServer();

auto g_option = std::make_shared<Option>();
int main(int argc, char *argv[])
{
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;

    if (argc > 2)
    {
        strncpy(g_option->ip_remote, argv[2], sizeof(g_option->ip_remote));
        strncpy(g_option->ip_local, argv[2], sizeof(g_option->ip_local));
    }

    if (argc > 1)
    {
        if (!strcmp(argv[1], "-s"))
        {
            StartServer();
        }
        else if (!strcmp(argv[1], "-c"))
        {
            StartClient();
        }
    }

    return 0;
}

void StartClient()
{
    NetSnoopClient client(g_option);
    client.OnStopped = [](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
        std::clog << "client finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
    };
    auto t = std::thread([&client]() {
        LOGV("init_client\n");
        client.Run();
    });
    t.join();
}

void StartServer()
{
    static int count = 0;
    NetSnoopServer server(g_option);
    server.OnAcceptNewPeer = [&](Peer *peer) {
        count++;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip, port);
        std::clog << "peer connect: [" << count << "]: " << ip.c_str() << ":" << port << std::endl;
    };
    server.OnClientDisconnect = [&](Peer *peer) {
        count--;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip, port);
        std::clog << "peer disconnect: [" << count << "]: " << ip.c_str() << ":" << port << std::endl;
    };
    auto t = std::thread([&]() {
        LOGV("init_server\n");
        server.Run();
    });
    t.detach();

    std::string cmd;
    while (true)
    {
        std::cout << "Input Action:";
        std::getline(std::cin, cmd);
        if (cmd.length() < 4)
            continue;
        auto command = CommandFactory::New(cmd);
        if (command)
        {
            command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::clog << "command finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            });
        }
        server.PushCommand(command);
    }
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