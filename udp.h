#pragma once

#include "sock.h"

class Udp : public Sock
{
public:
    Udp();
    Udp(int fd);
    int Listen(int count) override;
    int Accept() override;
    ssize_t SendTo(const std::string& buf,sockaddr_in* sockaddr);
    ssize_t RecvFrom(std::string& buf,sockaddr_in* sockaddr);


    /**
     * @brief Used when you need to send multicast data.
     * set IP_MULTICAST_IF and IP_MULTICAST_LOOP
     * 
     * @param interface_addr 
     * @return int 
     */
    int BindMulticastInterface(std::string interface_addr="0.0.0.0");

    /**
     * @brief Used when you need to recv multicast data.
     * Join multicast group with specific interface.
     * 
     * @param group_addr 
     * @param interface_addr 
     * @return int 
     */
    int JoinMUlticastGroup(std::string group_addr,std::string interface_addr="0.0.0.0");

    /**
     * @brief Leave a multicast group.
     * 
     * @param group_addr 
     * @param interface_addr 
     * @return int 
     */
    int DropMulticastGroup(std::string group_addr,std::string interface_addr="0.0.0.0");

private:

    int count_;
    
    DISALLOW_COPY_AND_ASSIGN(Udp);
};

