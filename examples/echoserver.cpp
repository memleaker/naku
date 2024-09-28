#include <cstring>
#include <naku/naku.h>

naku::netio_task echo(naku::tcp::conn c)
{
    ssize_t n;
    char buf[1024];

    for (;;)
    {
        ::memset(buf, 0, sizeof(buf));
        n = c.read(buf, sizeof(buf)-1);
        if (n == -1) {
            c.shutdown();
            co_return -1;
        }

        c.write(buf, n);
    }

    co_return 0;
}

int main(void)
{
    naku::copool_init();

    int ret;
    uint16_t clientport;
    std::string clientip;
    naku::tcp::conn c;
    naku::tcp::listener l;
    l.listen("0.0.0.0", 8888);

    for (;;)
    {
        ret = l.accept(clientip, clientport, c);
        if (ret == -1) {
            return -1;
        }

        naku::co_run(echo, c);
    }
}