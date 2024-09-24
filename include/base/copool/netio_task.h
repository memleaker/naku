#ifndef NAKU_NETIO_TASK_H
#define NAKU_NETIO_TASK_H

#include <cstdint>
#include <coroutine>

#include <sys/epoll.h>

namespace naku { namespace base {

/* @brief 协程运行状态 */
enum CO_STATE { CO_RUNNING, CO_IOWAIT};

/* @brief 将一个协程封装为一个netio_task任务 */
class netio_task {
public:
    class promise_type {
    public:
		promise_type() : fd(-1), run_state(CO_RUNNING), events(EPOLLIN) {}

        /* @brief 设置协程启动时挂起 */
        std::suspend_always initial_suspend() { return {}; }

        /* @brief 设置协程启动时的返回值 */
        netio_task get_return_object()
        { return {netio_task(std::coroutine_handle<netio_task::promise_type>::from_promise(*this))}; }
        
        /* @brief 设置协程结束(co_return)时挂起 */
        std::suspend_always final_suspend() noexcept { return {}; }

        /* @brief 设置协程结束时(co_return)返回值为int */
		void return_value(int status) {ret_status = status;}

        /* @brief 定义发生异常时的行为 */
        void unhandled_exception() { throw; }

    public:
		int fd;              /* @brief 保存等待IO事件的fd */
		int ret_status;      /* @brief 保存协程返回值 */
		CO_STATE run_state;  /* @beief 保存协程的运行状态 */
		uint32_t events;     /* @brief 保存要监控的事件 */
    };

public:
    /* @brief 保存控制协程的句柄 */
    std::coroutine_handle<netio_task::promise_type> handle_;
};

} } // namespace

#endif // NAKU_NETIO_TASK_H