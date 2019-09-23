#include <iostream>
#include <functional>
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <cmath>

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
        std::clog << "usage: \n"
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
        std::cout << "peer connect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server->OnPeerDisconnected = [&](const Peer *peer) {
        count--;
        std::cout << "peer disconnect(" << count << "): " << peer->GetCookie() << std::endl;
    };
    server->OnPeerStopped = [&](const Peer *peer, std::shared_ptr<NetStat> netstat) {
        std::cout << "peer stoped: (" << peer->GetCookie() << ") " << peer->GetCommand()->GetCmd().c_str()
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

    std::cout << "After all clients connected, press any key to start..." << std::endl;
    getchar();
    std::cout << "Let's go..." << std::endl;

    std::mutex mtx;
    std::condition_variable cv;

    // ping first
    {
        auto command = CommandFactory::New("ping count 20 wait 2000");
        command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat){
            std::cout << "command finish: " << oldcommand->GetCmd() << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            cv.notify_all();
        });
        server->PushCommand(command);
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);
    }
    
    std::shared_ptr<NetStat> maxstat;
    std::shared_ptr<NetStat> avgstat;
    std::string maxcommand;

    int CMD_COUNT = 0;
    int MAX_TIMES = 3;
    int times = 0;
begin:
    maxstat = NULL;
    std::vector<std::string> cmds = {
        "send unicast true speed 50 time 10000 size 1472 timeout 300 wait 2000",
        "send unicast true speed 100 time 10000 size 1472 timeout 300 wait 2000",
        "send unicast true speed 500 time 10000 size 1472 timeout 300 wait 5000",
        "send unicast true speed 1000 time 10000 size 1472 timeout 300 wait 5000",
        "send unicast true speed 2000 time 10000 size 1472 timeout 300 wait 5000",
        "send unicast true count 5000 interval 0 size 1472 timeout 300 wait 5000",

        "send multicast true speed 50 time 10000 size 1472 timeout 300 wait 2000",
        "send multicast true speed 100 time 10000 size 1472 timeout 300 wait 2000",
        "send multicast true speed 500 time 10000 size 1472 timeout 300 wait 5000",
        "send multicast true speed 1000 time 10000 size 1472 timeout 300 wait 5000",
        "send multicast true speed 2000 time 10000 size 1472 timeout 300 wait 5000",
        "send multicast true count 5000 interval 0 size 1472 timeout 300 wait 5000",
    };
    CMD_COUNT = cmds.size();
    for (auto i = 0; i < CMD_COUNT; i++)
    {
        times = 0;
        avgstat = NULL;
        if(i==CMD_COUNT/2)
        {
            maxstat = NULL;
        }
        for (auto k = 0; k < MAX_TIMES; k++)
        {
            auto command = CommandFactory::New(cmds[i]);
            command->RegisterCallback([&,i,k](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::cout << "command value: " << oldcommand->ToString() << std::endl;
                std::cout << "command finish: " << oldcommand->GetCmd() << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
                std::cout << "progress: " <<i*MAX_TIMES+k+1<<"/"<< CMD_COUNT*MAX_TIMES << std::endl;
                if(stat)
                {
                    if (!avgstat)
                    {
                        avgstat = stat;
                    }
                    else
                    {
                        *avgstat += *stat;
                    }
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
                            maxcommand = oldcommand->GetCmd();
                        }
                        else if (maxstat->recv_speed < avgstat->recv_speed)
                        {
                            maxstat = avgstat;
                            maxcommand = oldcommand->GetCmd();
                        }
                        std::cout << "avg recv_speed: " << oldcommand->GetCmd() << " || " << (avgstat ? avgstat->ToString() : "NULL") << std::endl;
                        std::cout << "max recv_speed: " << maxcommand << " || " << (maxstat ? maxstat->ToString() : "NULL") << std::endl;
                    }
                    std::cout << "----------------------------" << std::endl;
                }
                cv.notify_all();
            });
            server->PushCommand(command);
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock);
        }
    }
    
    // ping last
    {
        auto command = CommandFactory::New("ping count 20 wait 2000");
        command->RegisterCallback([&](const Command *oldcommand, std::shared_ptr<NetStat> stat){
            std::cout << "command finish: " << oldcommand->GetCmd() << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            cv.notify_all();
        });
        server->PushCommand(command);
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);
    }

    std::cout << "finished." << std::endl;
}

void StartClient()
{
    int result;
    std::vector<std::string> ips;
    ips.push_back(g_option->ip_remote);
    int scale = 4;
    while (true)
    {
        for (auto it = ips.rbegin();it!=ips.rend();it++)
        {
            auto ip = *it;
            memset(g_option->ip_remote, 0, sizeof(g_option->ip_remote));
            strncpy(g_option->ip_remote, ip.c_str(), sizeof(g_option->ip_remote) - 1);

            NetSnoopClient client(g_option);
            client.OnConnected = [&] {
                std::clog << "connect to " << g_option->ip_remote << ":" << g_option->port << " (" << g_option->ip_multicast << ")" << std::endl;
                scale = 0;
            };
            client.OnStopped = [](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
                std::cout << "peer finish: " << oldcommand->GetCmd() << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            };

            LOGVP("client running...");
            client.Run();
            std::clog << "client stop, restarting..." << std::endl;
            std::clog << "----------------------------" << std::endl;
        }

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
            
            std::string server_ip(40, 0);
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(multicast.GetFd(),&readfds);
            int wait_seconds = std::pow(2,scale);
            timeval timeout{wait_seconds};
            if(scale<10) scale++;
            std::clog << "finding server...(timeout="<< wait_seconds <<")"<< std::endl;
            result = select(multicast.GetFd()+1,&readfds,NULL,NULL,&timeout);
            if(result<0)
            {
                PSOCKETERROR("select error");
                continue;
            }
            if(result==0)
            {
                LOGDP("select timeout.");
                continue;
            }
            result = multicast.RecvFrom(server_ip, &server_addr);
            if(result<=0) continue;
            server_ip.resize(result);
            if(server_ip == "0.0.0.0")
            {
                server_ip = inet_ntoa(server_addr.sin_addr);
            }
            std::clog << "find server: " << server_ip << std::endl;
            ips.push_back(server_ip);
        }
    }
}
