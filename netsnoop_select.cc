#include <iostream>
#include <functional>
#include <thread>
#include <mutex>
#include <memory>
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
    auto server = std::make_shared<NetSnoopServer>(g_option);
    server->OnPeerConnected = [&](const Peer *peer) {
        count++;
        std::clog << "peer connect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server->OnPeerDisconnected = [&](const Peer *peer) {
        count--;
        std::clog << "peer disconnect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server->OnPeerStopped = [&](const Peer *peer, std::shared_ptr<NetStat> netstat) {
        std::cout << "peer stoped: (" << peer->GetCookie() << ") " << peer->GetCommand()->cmd.c_str()
                  << " || " << (netstat ? netstat->ToString() : "NULL") << std::endl;
    };
    auto server_thread = std::thread([server]() {
        LOGVP("server running...");
        server->Run();
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

    std::clog << "After all clients connected, press any key to start..." << std::endl;
    getchar();
    std::clog << "Let's go..." << std::endl;

    std::mutex mtx;
    std::condition_variable cv;

    // recv count 100 interval 0 size 1024
    std::string cmd(MAX_CMD_LENGTH, 0);
    std::shared_ptr<NetStat> maxstat;
    std::shared_ptr<NetStat> avgstat;
    std::string maxcommand;

    int MAX_DELAYS_TIMES = 10;
    int MAX_TIMES = 3;
    int times = 0;
    bool finish = false;
    bool is_multicast = true;
    int size = 0;
begin:
    maxstat = NULL;
    for (auto i = MAX_DELAYS_TIMES; i >= 0; i--)
    {
        times = 0;
        avgstat = NULL;
        if (is_multicast)
            size = sprintf(&cmd[0], "send multicast true count 100 interval %d size 1472 wait 500", i*3);
        else
            size = sprintf(&cmd[0], "send unicast true count 100 interval %d size 1472 wait 500", i*3);
        for (auto k = 0; k < MAX_TIMES; k++)
        {
            auto command = CommandFactory::New(cmd.substr(0, size));
            command->RegisterCallback([&,i,k](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::cout << "command finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
                std::clog << "progress: " <<(MAX_DELAYS_TIMES-i)*MAX_TIMES+k+1<<"/"<< (MAX_DELAYS_TIMES+1)*MAX_TIMES << std::endl;
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
                        std::cout << "avg recv_speed: " << oldcommand->cmd << " || " << (avgstat ? avgstat->ToString() : "NULL") << std::endl;
                        std::cout << "max recv_speed: " << maxcommand << " || " << (maxstat ? maxstat->ToString() : "NULL") << std::endl;
                        std::cout << "----------------------------" << std::endl;
                    }
                    cv.notify_all();
                }
            });
            server->PushCommand(command);
        }
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return times >= MAX_TIMES; });
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
            // the IGMP package will send from default route.
            result = multicast.JoinMUlticastGroup("239.3.3.4");
            if(result<0)
            {
                std::clog << "join multicast group 239.3.3.4("<<"0.0.0.0"<<") error, retry in 3 seconds..." << std::endl;
                //TODO: fix this,in chromeos,it's too quick to start before if up.
                sleep(3);
                continue;
            }
            // this way will case obscure problem, when the system is not conneted to any network while has local ip.
            // auto ips = multicast.GetLocalIps();
            // for (auto &&ip : ips)
            // {
            //     result = multicast.JoinMUlticastGroup("239.3.3.4",ip);
            //     if(result<0)
            //     {
            //         std::clog << "join multicast group 239.3.3.4("<<ip<<") error, retry in 3 seconds..." << std::endl;
            //         continue;
            //     }
            // }
            
            std::clog << "finding server... " << std::endl;
            std::string server_ip(40, 0);
            result = multicast.RecvFrom(server_ip, &server_addr);
            ASSERT(result > 0);
            server_ip.resize(result);
            if(server_ip == "0.0.0.0")
            {
                server_ip = inet_ntoa(server_addr.sin_addr);
            }
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
        std::clog << "client stop, restarting in 3 seconds..." << std::endl;
        std::clog << "----------------------------" << std::endl;
    }
}
