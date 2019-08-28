#include "netsnoop.h"
#include "udp.h"

int main(int argc,char* argv[])
{
    Logger::SetGlobalLogLevel(LogLevel::LLDEBUG);
    int result;

    Udp udp;
    result = udp.Initialize();
    if(argc>2 && !strcmp("-s",argv[1]))
    {
        auto addr = inet_addr(argv[2]);
        result = setsockopt(udp.GetFd(), IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr));
        ASSERT_RETURN(result >= 0, -1);

        char loopch = 1;
        result = setsockopt(udp.GetFd(), IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch));
        ASSERT_RETURN(result >= 0, -1);

        result = udp.Connect("239.3.3.3",4000);
        ASSERT_RETURN(result >= 0, -1, "multicast socket connect server error.");

        while (true)
        {
            udp.Send("aaaaa",5);
            sleep(1);
        }
    }
    else
    {
        udp.Bind("0.0.0.0",4000);
        join_mcast(udp.GetFd(),"239.3.3.3",argv[2]);
        while (true)
        {
            char buf[100] = {0};
            result = udp.Recv(buf,100);
            ASSERT(result>=0);
        }
    }
    
    return 0;
}