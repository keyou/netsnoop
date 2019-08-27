#include <iostream>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>

#include <mutex> // For std::unique_lock
#include <thread>
#include <condition_variable>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

void RunTest(NetSnoopServer *server, int);
void StartClients(int count, bool join);
void StartServer();

std::vector<std::string> cmds{
    "ping",
    "send",
    "send count 100 interval 1 size 1024",
    "send count 10000 interval 0 size 1024",
    "ping count 10 interval 200 size 1024",
    "ping count 10 interval 200 size 10240",
    "send count 1000 interval 1 size 1024",
    "send count 1000 interval 0 size 8096",
    "send count 10000 interval 0 size 12024",
    "send count 1000 interval 1 size 20240"};

std::shared_ptr<Option> g_option = std::make_shared<Option>();
int main(int argc, char *argv[])
{
    Logger::SetGlobalLogLevel(LogLevel::LLDEBUG);
    strncpy(g_option->ip_remote, "0.0.0.0", sizeof(g_option->ip_remote) - 1);
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
            StartClients(10,true);
        }
        return 0;
    }

    int MAX_CLIENT_COUNT = argc > 1 ? atoi(argv[1]) : 10;
    int MAX_CMD_COUNT = argc > 2 ? atoi(argv[2]) : 0;
    int client_count = 0;

    std::mutex mtx;
    std::condition_variable cv;

    NetSnoopServer server(g_option);
    server.OnPeerConnected = [&](const Peer *peer) {
        std::unique_lock<std::mutex> lock;
        client_count++;
        // All client has connected.
        if (client_count == MAX_CLIENT_COUNT)
            cv.notify_all();
    };
    server.OnServerStart = [&](const NetSnoopServer *server) {
        StartClients(MAX_CLIENT_COUNT, false);
    };
    auto server_thread = std::thread([&server] {
        LOGVP("init_server");
        server.Run();
    });
    server_thread.detach();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return client_count == MAX_CLIENT_COUNT; });
    RunTest(&server, MAX_CMD_COUNT);

    std::cout << "exit" << std::endl;
    return 0;
}

void RunTest(NetSnoopServer *server, int cmd_count)
{
    if (cmd_count != 0)
        cmds.erase(cmds.begin() + cmd_count, cmds.end());

    std::mutex mtx;
    std::condition_variable cv;
    int i = 0;
    int j = 0;
    for (auto &cmd : cmds)
    {
        std::cerr << "============ " << i++ << " =============" << std::endl;
        std::cerr << "cmd: " << cmd << std::endl;
        auto command = CommandFactory::New(cmd);
        if (!command)
        {
            LOGEP("error command: %s", cmd.c_str());
            continue;
        }
        command->RegisterCallback([&, i](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
            std::clog << "command finish: [" << i << "/" << cmds.size() << "] " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            std::unique_lock<std::mutex> lock(mtx);
            j++;
            if (i == cmds.size())
                cv.notify_all();
        });
        server->PushCommand(command);
    }
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return j == cmds.size(); });
}

void StartServer()
{
    static int count = 0;
    std::shared_ptr<NetSnoopServer> server = std::make_shared<NetSnoopServer>(g_option);
    server->OnPeerConnected = [&](const Peer *peer) {
        count++;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip,port);
        std::clog << "peer connect: [" << count <<"]: "<<ip.c_str()<<":"<<port<< std::endl;
    };
    server->OnPeerDisconnected = [&](const Peer* peer){
        count--;
        std::string ip;
        int port;
        peer->GetControlSock()->GetPeerAddress(ip,port);
        std::clog << "peer disconnect: [" << count <<"]: "<<ip.c_str()<<":"<<port<< std::endl;
    };
    auto thread = std::thread([&] {
        LOGVP("start server.");
        server->Run();
    });
    
    std::cout << "Press Enter to start." << std::endl;
    getchar();
    while (true)
    {
        int i = 0;
        for (auto &cmd : cmds)
        {
            i++;
            auto command = CommandFactory::New(cmd);
            command->RegisterCallback([&, i](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::clog << "command finish: [" << i << "/" << cmds.size() << "] " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            });
            server->PushCommand(command);
        }
        std::this_thread::sleep_for(std::chrono::seconds(300));
    }
    thread.join();
}

void StartClients(int count, bool join)
{
    LOGVP("start clients.(count=%d)", count);
    std::mutex mtx;

    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++)
    {
        auto client_thread = std::thread([&, i]() {
            LOGVP("init_client %d", i);
            NetSnoopClient client(g_option);
            client.OnStopped = [&,i](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
                std::unique_lock<std::mutex> lock(mtx);
                std::clog << "client ["<<i+1<<"] finish: " << oldcommand->cmd << " || " << (stat ? stat->ToString() : "NULL") << std::endl;
            };
            client.Run();
        });
        threads.push_back(std::move(client_thread));
    }
    if (join)
    {
        for (auto &thread : threads)
        {
            thread.join();
        }
    }
}