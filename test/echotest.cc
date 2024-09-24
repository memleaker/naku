#include "naku.h"
#if 0
void Echo(naku::tcpconn c)
{
    ssize_t n;
    char buf[1024];

    for (;;)
    {
        n = c.read(buf, sizeof(buf));
        if (n>0) {
            c.write(buf, n);
        } else {
            break;
        }
    }
}

void EchoServer()
{
    naku::listener l;
    l.listen("0.0.0.0", 8080);
  
    for (;;)
    {
        naku::tcpconn c = l.accept(); 
        naku::co_run(Echo, c);
    }
}
#endif
int main()
{
    naku::co_init();

    naku::co_run(EchoServer);

    for(;;) sleep(1);
}