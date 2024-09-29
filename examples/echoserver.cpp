#include <cstring>
#include <naku/naku.h>

naku::netio_task echo(naku::tcp::conn c)
{
    ssize_t n;
    char buf[1024];

    for (;;)
    {
        ::memset(buf, 0, sizeof(buf));
        std::cout << "wait read" << std::endl;
        n = c.read(buf, sizeof(buf)-1);
        std::cout << "read end" << std::endl;
        if (n == -1) {
            c.shutdown();
            co_return -1;
        }

        c.write(buf, n);
    }

    co_return 0;
}

#include <unistd.h>

int main(void)
{
    naku::copool_init();

    //sleep(10); // 测试等待一会再提交任务是否正常

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
            std::cout << "accept failed: " << strerror(errno) << std::endl;
            return -1;
        }

        std::cout << "accept new connection from " << clientip << ":" << clientport << std::endl;


        /* 方案存在问题:  
         * 1. 我们要的是把echo函数也作为一个协程运行, 这样可以实现类似go语言的网络编程
         * 2. 但是echo函数会卡住, 因此我们不能封装listener和conn, 但不封装很难用
         *    而且是不支持嵌套协程, 虽然能创建, 但是一旦卡住就导致无法调度了
         * 3. 在此案例中 echo 卡在了 读取数据这里, 而收数据的协程也不会被调度到了, 就卡死了
         * 4. 通过优先队列获取任务最少的线程功能还未实现, 还需实现
         * 5. 要考虑是否使用非对称协程了, c++的协程就是有这个问题
         */
        naku::co_run(echo, c);
    }
}