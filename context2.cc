
#include "context2.h"

Context::Context() : max_fd(-1), control_fd(-1), data_fd(-1)
{
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
}

void Context::SetReadFd(int fd)
{
    FD_SET(fd, &read_fds);
    max_fd = std::max(max_fd, fd);
}

void Context::SetWriteFd(int fd)
{
    FD_SET(fd, &write_fds);
    max_fd = std::max(max_fd, fd);
}

void Context::ClrReadFd(int fd)
{
    FD_CLR(fd, &read_fds);
}

void Context::ClrWriteFd(int fd)
{
    FD_CLR(fd, &write_fds);
}
