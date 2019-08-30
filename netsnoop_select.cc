#include <iostream>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

void StartClient();
void StartServer();

auto g_option = std::make_shared<Option>();

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: \n"
                     "   netsnoop_select -s 0.0.0.0 4000 -vvv\n";
        return 0;
    }
    Logger::SetGlobalLogLevel(LLERROR);

    SockInit init;

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
        if (!strcmp(argv[1], "-c"))
        {
            StartClient();
        }
    }

    return 0;
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
    auto server_thread = std::thread([&]() {
        LOGVP("server running...");
        server.Run();
    });
    server_thread.detach();

    auto notify_thread = std::thread([] {
        LOGVP("notify running...");
        Udp multicast;
        multicast.Initialize();
        multicast.Connect("239.3.3.4", 4001);
        while (true)
        {
            multicast.Send(g_option->ip_local, strlen(g_option->ip_local));
            sleep(3);
        }
    });
    notify_thread.detach();

    std::cout << "After all clients connected, press any key to start..." << std::endl;
    getchar();
    std::cout << "Let's go..." << std::endl;

    std::mutex mtx;
    std::condition_variable cv;

    // recv count 100 interval 0 size 1024
    std::string cmd(MAX_CMD_LENGTH, 0);
    std::shared_ptr<NetStat> maxstat;
    std::shared_ptr<NetStat> avgstat;
    std::string maxcommand;

    int MAX_TIMES = 3;
    int times = 0;
    bool finish = false;
    bool is_multicast = true;
    int size = 0;
begin:
    maxstat = NULL;
    for (auto i = 30; i >= 0; i -= 3)
    {
        times = 0;
        avgstat = NULL;
        if (is_multicast)
            size = sprintf(&cmd[0], "send multicast true count 100 interval %d size 1472 wait 500", i);
        else
            size = sprintf(&cmd[0], "send count 100 interval %d size 1472 wait 500", i);
        for (auto k = 0; k < MAX_TIMES; k++)
        {
            auto command = CommandFactory::New(cmd.substr(0, size));
            command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::clog << "command finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
                if (!avgstat)
                {
                    avgstat = stat;
                }
                else
                {
                    *avgstat += *stat;
                }
                times++;
                if (times >= MAX_TIMES)
                {
                    if (avgstat)
                    {
                        *avgstat /= MAX_TIMES;
                        if (!maxstat)
                        {
                            maxstat = avgstat;
                            maxcommand = oldcommand->cmd;
                        }
                        else if (maxstat->recv_speed < avgstat->recv_speed)
                        {
                            maxstat = avgstat;
                            maxcommand = oldcommand->cmd;
                        }
                        else
                        {
                            //finish = true;
                        }
                        std::clog << "avg recv_speed: " << oldcommand->cmd << " || " << (avgstat ? avgstat->ToString() : "NULL") << std::endl;
                        std::clog << "max recv_speed: " << maxcommand << " || " << (maxstat ? maxstat->ToString() : "NULL") << std::endl;
                        std::clog << "----------------------------" << std::endl;
                    }
                    cv.notify_all();
                }
            });
            server.PushCommand(command);
        }
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return times >= MAX_TIMES; });
        //if(finish) break;
    }
    if (is_multicast)
    {
        std::clog << "multicast finished." << std::endl;
        is_multicast = false;
        goto begin;
    }
    else
        std::clog << "unicast finished." << std::endl;
}

void StartClient()
{
    int result;

    while (true)
    {
        {
            sockaddr_in server_addr;
            Udp multicast;
            result = multicast.Initialize();
            result = multicast.Bind("0.0.0.0", 4001);
            result = multicast.JoinMUlticastGroup("239.3.3.4");

            std::clog << "finding server... " << std::endl;
            std::string server_ip(40, 0);
            result = multicast.RecvFrom(server_ip, &server_addr);
            ASSERT(result > 0);
            server_ip = inet_ntoa(server_addr.sin_addr);
            std::clog << "find server: " << server_ip << std::endl;
            memset(g_option->ip_remote, 0, sizeof(g_option->ip_remote));
            strncpy(g_option->ip_remote, server_ip.c_str(), sizeof(g_option->ip_remote) - 1);
        }

        NetSnoopClient client(g_option);
        client.OnConnected = [] {
            std::clog << "connect to " << g_option->ip_remote << ":" << g_option->port << " (" << g_option->ip_multicast << ")" << std::endl;
        };
        client.OnStopped = [](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
            std::cout << "peer finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
        };

        LOGVP("client running...");
        client.Run();
        std::clog << "client stop, restarting..." << std::endl;
        std::clog << "----------------------------" << std::endl;
    }
}
