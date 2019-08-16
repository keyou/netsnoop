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
    "echo interval 0",
    "recv",
    "recv count 100 interval 1 size 1024",
    "recv count 10000 interval 0 size 1024",
    "echo count 10 interval 200 size 1024",
    "echo count 10 interval 200 size 10240",
    "recv count 1000 interval 1 size 1024",
    "recv count 1000 interval 1 size 10240",
    "recv count 1000 interval 1 size 20240"};

std::shared_ptr<Option> g_option;
int main(int argc, char *argv[])
{
    std::cout << "netsnoop test begin." << std::endl;
    g_option = std::make_shared<Option>();
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;

    if (argc > 1 && strncmp("-s", argv[1], 2) == 0)
    {
        StartServer();
        return 0;
    }
    else if (argc > 1 && strncmp("-c", argv[1], 2) == 0)
    {
        int count = 1;
        if (argc > 2)
        {
            count = atoi(argv[2]);
        }
        if (argc > 3)
        {
            strncpy(g_option->ip_remote, argv[3], sizeof(g_option->ip_remote));
        }
        StartClients(count, true);
        return 0;
    }

    int MAX_CLIENT_COUNT = argc > 1 ? atoi(argv[1]) : 10;
    int MAX_CMD_COUNT = argc > 2 ? atoi(argv[2]) : 0;
    int client_count = 0;

    std::mutex mtx;
    std::condition_variable cv;

    NetSnoopServer server(g_option);
    server.OnAcceptNewPeer = [&](Peer *peer) {
        std::unique_lock<std::mutex> lock;
        client_count++;
        // All client has connected.
        if (client_count == MAX_CLIENT_COUNT)
            cv.notify_all();
    };
    server.OnServerStart = [&](NetSnoopServer *server) {
        StartClients(MAX_CLIENT_COUNT, false);
    };
    auto server_thread = std::thread([&server] {
        LOGV("init_server\n");
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
            LOGE("error command: %s\n", cmd.c_str());
            continue;
        }
        command->RegisterCallback([&, i](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
            std::clog << "command finish: [" << i << "/" << cmds.size() << "] " << oldcommand->cmd << " >> " << (stat ? stat->ToString() : "NULL") << std::endl;
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
    auto thread = std::thread([&] {
        LOGV("start server.\n");
        std::shared_ptr<NetSnoopServer> server = std::make_shared<NetSnoopServer>(g_option);
        server->OnAcceptNewPeer = [&](Peer *peer) {
            static int count = 0;
            count++;
            std::clog << "peer connect: " << count << std::endl;
        };
        server->Run();
    });
    int result;
    std::string ip = g_option->ip_remote;
    int port = g_option->port;
    Udp udp;
    result = udp.Initialize();
    udp.Connect(ip, port);

    while (true)
    {
        std::cout << "Press Enter to start." << std::endl;
        getchar();
        int i = 0;
        for (auto &cmd : cmds)
        {
            i++;
            auto command = CommandFactory::New(cmd);
            command->RegisterCallback([&, i](const Command *oldcommand, std::shared_ptr<NetStat> stat) {
                std::clog << "command finish: [" << i << "/" << cmds.size() << "] " << oldcommand->cmd << " >> " << (stat ? stat->ToString() : "NULL") << std::endl;
            });
            //server->PushCommand(command);
            result = udp.Send(command->cmd.c_str(), command->cmd.length());
            std::string cmd2(MAX_CMD_LENGTH, 0);
            result = udp.Recv(&cmd2[0], cmd2.length());
            ASSERT(result == cmd.length());
            cmd2.resize(result);
            LOGV("pushed cmd: %s\n", cmd2.c_str());
        }
    }
    thread.join();
}

void StartClients(int count, bool join)
{
    LOGV("start clients.(count=%d)\n", count);
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++)
    {
        auto client_thread = std::thread([&, i]() {
            LOGV("init_client %d\n", i);
            NetSnoopClient client(g_option);
            client.OnStopped = [i](std::shared_ptr<Command> oldcommand, std::shared_ptr<NetStat> stat) {
                static int index = 0;
                index++;
                std::clog << "command finish: [" << index << "/" << cmds.size() << "] " << oldcommand->cmd << " >> " << (stat ? stat->ToString() : "NULL") << std::endl;
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