#include "base/poller/epoller.h"

namespace naku { namespace base {

/* @brief 添加IO事件监控 */
int epoller::ioevent_add(int fd, uint32_t events, void *pridata)
{
    int ret;
    epoll_event ev;

    /* @brief
        *  1. 对于传入的fd需要判断使用 ADD还是MOD
        *  2. 如果使用 std::set 保存所有的fd:
        *    (1) 向set添加fd可以但不知何时删除fd
        *	  (2) 耗费了空间保存fd
        *	  (3) 查找与epoll复杂度相同, 都是红黑树
        *  3. 因此对于传入的fd，先使用MOD操作(MOD频繁),如果报错 ENOENT
        *     那么再使用ADD操作加入到epoll中(libevent即是这样做的) 
        */
    ev.events   = events | EPOLLONESHOT;
    ev.data.ptr = pridata;

    ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    if (ret == 0) {
        return 0;
    } else {
        if (errno != ENOENT) {
            LOG_ERROR << "Epoll MOD fd failed : " << strerror(errno) << std::endl;
            return -1;
        }
    }

    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (ret == -1) {
        LOG_ERROR << "Epoll ADD fd failed : " << strerror(errno) << std::endl;
        return -1;
    }

    return 0;
}

/* @brief 当关闭fd时, 会自动从epoll中移除, 因此该函数不常调用 */ 
int epoller::ioevent_del(int fd)
{
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

/* @brief 监控IO事件, 并设置协程运行状态 */
int epoller::ioevent_handle(void)
{
    int i, n;
    epoll_event evs[4096];

again:
    n = epoll_wait(epoll_fd, evs, 4096, 1);
    if (n == -1)
    {
        if (errno == EINTR)
            goto again;

        LOG_ERROR << "epoll wait failed: " << std::strerror(errno) << std::endl;
        return -1;
    }

    /* traverse events */
    for (i = 0; i < n; i++)
    {
        if (callback) {
            callback(evs[i].data.ptr);
        }
    }

    return 0;
}

} } // namespace