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

void RunTest(NetSnoopServer *server);

int main(int argc, char *argv[])
{
    std::cout << "netsnoop test begin." << std::endl;
    auto g_option = std::make_shared<Option>();
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;
    g_option->rate = 2048;
    g_option->buffer_size = 1024 * 8;

    constexpr int MAX_CLIENT_COUNT = 1;
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
        for (int i = 0; i < MAX_CLIENT_COUNT; i++)
        {
            auto client_thread = std::thread([&g_option, i]() {
                NetSnoopClient client(g_option);
                LOGV("init_client %d\n", i);
                client.Run();
            });
            client_thread.detach();
        }
    };
    auto server_thread = std::thread([&server] {
        LOGV("init_server\n");
        server.Run();
    });
    server_thread.detach();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return client_count == MAX_CLIENT_COUNT; });
    RunTest(&server);

    std::cout << "exit" << std::endl;
    return 0;
}

void RunTest(NetSnoopServer *server)
{
    std::vector<std::string> cmds{
        "recv count 100 interval 1",
        "recv count 100 interval 0",
        "recv count 10000 interval 0",
        "echo count 5 interval 200 size 1024",
        "echo count 5 interval 200 size 1024",
        "recv",
        "recv count 5 interval 200 size 1024",
        "recv count 5 interval 200 size 1024",
        "echo", "recv",
        "echo", "recv",
        "echo", "recv"};

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
            std::cout << "command stop: [" << i << "] " << oldcommand->cmd<< " >> "<<stat->ToString() << std::endl;
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