#include <iostream>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>

#include <mutex>  // For std::unique_lock
#include <shared_mutex>
#include <thread>

#include "netsnoop.h"
#include "net_snoop_client.h"
#include "net_snoop_server.h"

int main(int argc, char *argv[])
{
    std::cout << "netsnoop test begin." << std::endl;
    auto g_option = std::make_shared<Option>();
    strncpy(g_option->ip_remote, "127.0.0.1", sizeof(g_option->ip_remote));
    strncpy(g_option->ip_local, "0.0.0.0", sizeof(g_option->ip_local));
    g_option->port = 4000;
    g_option->rate = 2048;
    g_option->buffer_size = 1024 * 8;

    NetSnoopServer server(g_option);
    auto server_thread = std::thread([&server] {
        LOGV("init_server\n");
        server.Run();
    });
    server_thread.detach();

    for (int i = 0; i < 1; i++)
    {
        auto client_thread = std::thread([&g_option,i]() {
            NetSnoopClient client(g_option);
            LOGV("init_client %d\n",i);
            client.Run();
        });
        client_thread.detach();
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::vector<std::string> cmds{
        "echo",
        "echo count 5 interval 200 size 1024",
        "echo count 5 interval 200 size 1024",
        "recv",
        "recv count 5 interval 200 size 1024",
        "recv count 5 interval 200 size 1024",
        "echo", "recv",
        "echo", "recv",
        "echo", "recv"
    };

    std::mutex mtx;
    int i = 0;
    for(auto& cmd:cmds)
    {
        //mtx.lock();
        std::cout<<"============ "<<i++<<" ============="<<std::endl;
        std::cout<<"cmd: "<<cmd<<std::endl;
        auto command = CommandFactory::New(cmd);
        if (!command)
        {
            LOGE("error command: %s\n", cmd.c_str());
            continue;
        }
        command->RegisterCallback([&,i](const Command * oldcommand, std::shared_ptr<NetStat> stat){
            std::cout<<"command stop: "<<i<<oldcommand->cmd<<std::endl;
            //mtx.unlock();
        });
        server.PushCommand(command);
    }
    std::unique_lock<std::mutex> lock(mtx);
    return 0;
}
