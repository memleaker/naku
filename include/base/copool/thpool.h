#ifndef NAKU_THREAD_POOL_H
#define NAKU_THREAD_POOL_H

#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <utility>
#include <vector>

#include "base/copool/task_queue.h"

namespace naku { namespace base {

class thread_pool
{
private:
    /* 线程工作类 */
    class thread_worker
    {
    private:
        thread_pool *m_pool;
    public:
        thread_worker(thread_pool *pool) : m_pool(pool) {}

        /* 重载()运算符执行: 从队列中取任务并执行 */
        void operator()()
        {
            bool dequeued;
            std::function<void()> func;
            while (!m_pool->m_shutdown)
            {
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
                    if (m_pool->m_queue.empty())
                    {
                        m_pool->m_conditional_lock.wait(lock);
                    }
                    dequeued = m_pool->m_queue.dequeue(func);
                }

                if (dequeued)
                    func();
            }
        }
    };

public:
    thread_pool(const int n_threads = 4)
        : m_shutdown(true), m_threads(std::vector<std::thread>(n_threads)) {}

    /* 禁用拷贝和移动 */
    thread_pool(const thread_pool &) = delete;
    thread_pool(thread_pool &&) noexcept = delete;
    thread_pool &operator=(const thread_pool &) = delete;
    thread_pool &operator=(thread_pool &&) noexcept = delete;

    void init(void)
    {
        m_shutdown = false;
        for (auto& th : m_threads)
        {
            th = std::thread(thread_worker(this));
        }
    }

    void shutdown(void)
    {
        m_shutdown = true;
        m_conditional_lock.notify_all();

        for (auto &th : m_threads)
        {
            if (th.joinable())
                th.join();
        }
    }

	template <typename F, typename... Args>
	auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
	{
        /* 注意: 1. decltype(f(args...)) (), 分析获取到返回值类型
                2. 然后加上()无参数, 参数已被bind, 不需要额外参数 */
		std::function<decltype(f(args...)) ()> func = \
			std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        /* 为实现获取线程池返回值, 使用future特性 */
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        /* 使用 lambda 封装任务为  void()类型 */ 
		std::function<void()> wrapper_func = [task_ptr]() {
			(*task_ptr)();
		};

        /* 任务入队 */
		m_queue.enqueue(wrapper_func);
		m_conditional_lock.notify_one();

        return task_ptr->get_future();
	}

private:
    bool m_shutdown;
    task_queue<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;
    std::mutex m_conditional_mutex;
    std::condition_variable m_conditional_lock;
};

} } // namespace

#endif // NAKU_THREAD_POOL_H