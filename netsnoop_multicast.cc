#include "netsnoop.h"
#include "udp.h"

int main(int argc,char* argv[])
{
    Logger::SetGlobalLogLevel(LogLevel::LLVERBOSE);
    int result;
    if(argc<2)
    {
        std::clog<<"usage:\n"
                "    netsnoop_multicast -s 239.3.3.4 4001    (start server)\n"
                "    netsnoop_multicast -c 239.3.3.4 4001    (start client)\n"
                "    --------\n"
                "    You can set interface ip as below:\n"
                "    netsnoop_multicast -c 239.3.3.4 4001 [interface ip]\n";
        return 0;
    }

    SockInit init;
    std::string group_ip = "239.3.3.4";
    std::string interface_ip = "0.0.0.0";
    int group_port = 4001;

    if(argc>2)
    {
        group_ip = argv[2];
    }
    if(argc>3)
    {
        group_port = std::atoi(argv[3]);
    }
    if(argc>4)
    {
        interface_ip = argv[4];
    }
    
    Udp udp;
    result = udp.Initialize();
    ASSERT_RETURN(result>=0,-1);
    if(argc>1 && !strcmp("-s",argv[1]))
    {
        result = udp.BindMulticastInterface(interface_ip);
        ASSERT_RETURN(result >= 0, -1);
        result = udp.Connect(group_ip,group_port);
        ASSERT_RETURN(result >= 0, -1, "multicast socket connect server error.");

        while (true)
        {
            using namespace std::chrono;
            auto tp = high_resolution_clock::now();
            auto tm = system_clock::to_time_t(tp);
            auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count()%1000;
            char buf[100]={0};
            // %T is not supported by mingw-w64, why?
            if ((result = std::strftime(buf, sizeof(buf), "%m/%d %H:%M:%S", std::localtime(&tm)))>0) {
                std::ostringstream out;
                out<<buf<<"."<<ms;
                udp.Send(out.str().c_str(),out.str().length());
            }
            sleep(1);
        }
    }
    else if(argc>1&&!strcmp("-c",argv[1]))
    {
        udp.Bind("0.0.0.0",group_port);
        udp.JoinMUlticastGroup(group_ip,interface_ip);
        while (true)
        {
            char buf[100] = {0};
            result = udp.Recv(buf,100);
            ASSERT(result>=0);
        }
    }
    
    return 0;
}