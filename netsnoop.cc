#include <iostream>
#include <functional>
#include <thread>
#include <condition_variable>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

void StartClient();
void StartServer();

auto g_option = std::make_shared<Option>();

/**
 * @brief usage: 
 *      start server: netsnoop -s 0.0.0.0 4000 -vv
 *      start client: netsnoop -c 127.0.0.1 4000 -vv
 * 
 */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: \n"
                     "   netsnoop -s 0.0.0.0 4000 -vv   (start server)\n"
                     "   netsnoop -c 0.0.0.0 4000 -vv   (start client)\n"
                     "   --------\n"
                     "   command: \n"
                     "   ping count 10                  (test delay)\n"
                     "   send count 1000 interval 0     (test thoughput)\n";
        return 0;
    }

    ASSERT(init_sock() == 0);

#ifdef _DEBUG
    Logger::SetGlobalLogLevel(LLDEBUG);
#else
    Logger::SetGlobalLogLevel(LLERROR);
#endif // _DEBUG

    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote) - 1);
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local) - 1);
    strncpy(g_option->ip_multicast, "239.3.3.3", sizeof(g_option->ip_multicast) - 1);
    g_option->port = 4000;

    if (argc > 2)
    {
        strncpy(g_option->ip_remote, argv[2], sizeof(g_option->ip_remote) - 1);
        strncpy(g_option->ip_local, argv[2], sizeof(g_option->ip_local) - 1);
    }

    if (argc > 3)
    {
        g_option->port = atoi(argv[3]);
    }

    if (argc > 4)
    {
        Logger::SetGlobalLogLevel(LogLevel(LLERROR - strlen(argv[4]) + 1));
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

    clean_sock();
    return 0;
}

void StartClient()
{
    NetSnoopClient client(g_option);
    client.OnStopped = [](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
        std::clog << "peer finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
    };
    auto t = std::thread([&client]() {
        LOGVP("client run.");
        client.Run();
    });
    t.join();
}

void StartServer()
{
    static int count = 0;
    NetSnoopServer server(g_option);
    server.OnPeerConnected = [&](const Peer *peer) {
        count++;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip, port);
        LOGD << "peer connect: [" << count << "]: " << ip.c_str() << ":" << port;
    };
    server.OnPeerDisconnected = [&](const Peer *peer) {
        count--;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip, port);
        LOGD << "peer disconnect: [" << count << "]: " << ip.c_str() << ":" << port;
    };
    server.OnPeerStopped = [&](const Peer *peer, std::shared_ptr<NetStat> netstat) {
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip, port);
        LOGD << "peer stoped: (" << ip.c_str() << ":" << port << ") " << peer->GetCommand()->cmd.c_str()
             << " || " << (netstat ? netstat->ToString() : "NULL");
    };
    auto t = std::thread([&]() {
        LOGVP("server run.");
        server.Run();
    });
    t.detach();

    std::mutex mtx;
    std::condition_variable cv;

    std::string cmd;
    while (true)
    {
        std::cout << "command:";
        std::getline(std::cin, cmd);
        if(cmd.empty()) continue;
        auto command = CommandFactory::New(cmd);
        if (!command)
        {
            std::cout << "command '" << cmd << "' is not supported." << std::endl;
        }
        else
        {
            command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::cout << "command finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
                cv.notify_all();
            });
            server.PushCommand(command);
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock);
        }
        std::cout<<std::endl;
    }
}
