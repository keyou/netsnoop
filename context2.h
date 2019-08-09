#pragma once

#include <memory>
#include <functional>
#include <vector>

class Peer;

struct Context
{
    Context();

    void SetReadFd(int fd);
    void SetWriteFd(int fd);
    void ClrReadFd(int fd);
    void ClrWriteFd(int fd);

    int control_fd;
    int data_fd;
    fd_set read_fds;
    fd_set write_fds;
    int max_fd;
    std::vector<std::shared_ptr<Peer>> peers;
};