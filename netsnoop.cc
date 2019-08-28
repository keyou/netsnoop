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
    if (argc < 2 || !strcmp(argv[1], "-h"))
    {
        std::cout << "usage: \n"
                     "  netsnoop -s 0.0.0.0 4000            (start server)\n"
                     "  netsnoop -c <server ip> 4000        (start client)\n"
                     "  --------\n"
                     "  command:\n"
                     "  ping count 10                       (test delay)\n"
                     "  send count 1000                     (test thoughput)\n"
                     "  send count 1000 multicast true      (test multicast)\n"
                     "  \n"
                     "  version: "
                  << VERSION(0.1) << " (" << __DATE__ <<" "<< __TIME__ << ")" << std::endl;
        return 0;
    }

    SockInit init;

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

    return 0;
}

void StartClient()
{
    NetSnoopClient client(g_option);
    client.OnConnected = [] {
        std::clog << "connect to " << g_option->ip_remote << ":" << g_option->port << " (" << g_option->ip_multicast << ")" << std::endl;
    };
    client.OnStopped = [](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
        std::cout << "peer finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
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
        std::clog << "peer connect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server.OnPeerDisconnected = [&](const Peer *peer) {
        count--;
        std::clog << "peer disconnect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server.OnPeerStopped = [&](const Peer *peer, std::shared_ptr<NetStat> netstat) {
        std::clog << "peer stoped: (" << peer->GetCookie() << ") " << peer->GetCommand()->cmd.c_str()
                  << " || " << (netstat ? netstat->ToString() : "NULL") << std::endl;
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
        std::cout << "command:" << std::flush;
        std::getline(std::cin, cmd);
        if (cmd.empty())
            continue;
        auto command = CommandFactory::New(cmd);
        if (!command)
        {
            std::clog << "command '" << cmd << "' is not supported." << std::endl;
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
        std::cout << std::endl;
    }
}
