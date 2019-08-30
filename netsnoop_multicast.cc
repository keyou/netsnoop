#include "netsnoop.h"
#include "udp.h"

int main(int argc,char* argv[])
{
    Logger::SetGlobalLogLevel(LogLevel::LLVERBOSE);
    int result;

    SockInit init;

    Udp udp;
    result = udp.Initialize();
    ASSERT_RETURN(result>=0,-1);
    if(argc>2 && !strcmp("-s",argv[1]))
    {
        result = udp.BindMulticastInterface(argv[2]);
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
        udp.JoinMUlticastGroup("239.3.3.3",argv[2]);
        while (true)
        {
            char buf[100] = {0};
            result = udp.Recv(buf,100);
            ASSERT(result>=0);
        }
    }
    
    return 0;
}