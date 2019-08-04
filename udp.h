#pragma once

#include <string>

class Peer
{
public:
    Peer(const std::string ip,int port);
    std::string GetIp();
    int GetPort();
};

class Udp
{
public:
    Udp();
    int Bind();
    int Connect();
    int Initialize(int& fd,int recv_timeout=0);
    int Send(const std::string& data);
    int Recv(std::string& buffer);
    int SendTo(const std::string& data);
    int RecvFrom();
};

class UdpClient
{};

class UdpServer
{
public:
    UdpServer(std::string host,int port);
    int Initialize(int recv_timeout=0);
    int SendToAll(const std::string& data);
    int RecvFrom(std::string& buffer);
};


