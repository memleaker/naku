#ifndef NAKU_NAKU_H
#define NAKU_NAKU_H

#include <memory>

#include <naku/tcp.h>
#include <naku/base/copool/copool.h>
#include <naku/base/copool/netio_task.h>

namespace naku {

using netio_task = naku::base::netio_task;

/*
 * @brief 初始化协程池
 */
static inline void copool_init(void)
{
    naku::base::netco_pool::get_instance().init();
}

/*
 * @brief 等待协程池自己停止运行
 */
static inline void copool_wait(void)
{
    naku::base::netco_pool::get_instance().evloop();
}

/*
 * @brief 销毁协程池
 */
static inline void copool_shutdown(void)
{
    naku::base::netco_pool::get_instance().shutdown();
}

/*
 * @brief  创建新协程运行
 * @return 返回协程控制句柄
 */
template <typename F, typename... Args>
static inline netio_task co_run(F &&f, Args &&...args)
{
    return naku::base::netco_pool::get_instance().submit(std::forward<F>(f), std::forward<Args>(args)...);
}

/*
 * @brief 等待协程结束
 * @return 返回协程返回值
 */
static inline ssize_t co_wait(netio_task t)
{
    ssize_t n;

    t.handle_.promise().wait = true;
    t.handle_.promise().sem.acquire();
    n = t.handle_.promise().ret_status;
    t.handle_.destroy();

    return n;
}
}

#endif