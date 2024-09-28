#include <naku/tcp.h>
#include <naku/naku.h>
#include <naku/base/copool/netio_wrap.h>

#include <coroutine>

namespace naku { namespace tcp {

ssize_t conn::read(char *buf, size_t count)
{
    auto func = [this, buf, count](void) -> netio_task {
        ssize_t n = co_await naku::base::async_read(fd, buf, count);
        co_return n;
    };

    return co_wait(co_run(func));
}

ssize_t conn::write(char *buf, size_t count)
{
    auto func = [this, buf, count](void) -> netio_task {
        ssize_t n = co_await naku::base::async_write(fd, buf, count);
        co_return n;
    };

    return co_wait(co_run(func));
}

int listener::listen(std::string ip, uint16_t port)
{
    int ret;
    uint32_t addr;

    if (::inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return -1;
    }

    listenfd = naku::base::naku_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd == -1)
        return -1;

    ret = naku::base::naku_listen(listenfd, addr, port);
    if (ret == -1) {
        ::close(listenfd);
        return -1;
    }

    return 0;
}

int listener::accept(std::string &cliip, uint16_t& cliport, conn& c)
{
    auto func = [this, &cliip, &cliport](void) -> netio_task {
        int fd;
        sockaddr_in addr;
        socklen_t   len;
        char buf[INET_ADDRSTRLEN] = {0};

        fd = co_await naku::base::async_accept(fd, (sockaddr*)&addr, &len);
        if (fd == -1)
            co_return -1;
        
        ::inet_ntop(AF_INET, &addr.sin_addr.s_addr, buf, sizeof(buf));
        cliip   = buf;
        cliport = ::ntohs(addr.sin_port);

        co_return fd;
    };

    auto n = co_wait(co_run(func));
    if (n == -1) {
        return -1;
    }

    c = conn(n);
    return 0;
}

int dialer::dialto(std::string ip, uint16_t port, conn& c)
{
    uint32_t daddr;

    if (::inet_pton(AF_INET, ip.c_str(), &daddr) != 1) {
        return -1;
    }

    auto func = [daddr, port](void) -> netio_task {
        int ret;
        int fd;
        struct sockaddr_in addr;

        ::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = daddr;
        addr.sin_port = htons(port);

        fd = naku::base::naku_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd == -1)
            co_return -1;

        ret = co_await naku::base::async_connect(fd, (sockaddr*)&addr, sizeof(addr));
        if (ret == -1) {
            ::close(fd);
            co_return -1;
        }

        co_return 0;
    };

    auto n = co_wait(co_run(func));
    if (n == -1) {
        return -1;
    }

    c = conn(n);
    return 0;
}

}} // namespace