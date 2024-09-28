#ifndef NAKU_TCP_H
#define NAKU_TCP_H

#include <string>
#include <cstdint>
#include <unistd.h>

namespace naku { namespace tcp {

class conn
{
public:
    explicit conn(int _fd  = -1) : fd(_fd) {}

public:
    ssize_t read(char *buf, size_t count);
    ssize_t write(char *buf, size_t count);
    void shutdown(void) {::close(fd);}

private:
    int fd;
};

class listener
{
public:
    int listen(std::string ip, uint16_t port);
    int accept(std::string &cliip, uint16_t& cliport, conn& c);

private:
    std::string ip;
    uint16_t port;
    int listenfd;
};

class dialer
{
public:
    static int dialto(std::string ip, uint16_t port, conn& c);
};

}} // namespace

#endif