
#include "base/copool/copool.h"
#include "base/utils/utils.h"

namespace naku { namespace base {

/* @brief 新增IO事件进行监控 */
int netco_pool::iomul_worker::ioevent_add(netio_task *task, int events)
{
    return poll->ioevent_add(task->handle_.promise().fd, events, task);
}

/* @brief 对协程IO事件进行监控, 发生IO事件时修改协程状态 */
void netco_pool::iomul_worker::running(void)
{
    /* 设置回调函数, 发生事件时, 将协程状态从IOWAIT修改回RUNNING */
    auto callback = [](void *ptr) {
        netio_task *task;
        task = static_cast<netio_task*>(ptr);
        if (task) {
            task->handle_.promise().run_state = CO_RUNNING;
        }
    };

    poll->set_callback(callback);

    /* 启动线程, 开始监控事件 */
    th = std::thread([this]() {
        while (!pool->terminated)
        {
            if (poll->ioevent_handle() == -1)
            {
                LOG_ERROR << "poll thread exit!!!" << std::endl;
                return ;
            }
        }
    });
}

/* @brief 等待线程结束 */
void netco_pool::iomul_worker::stop(void)
{
    if (th.joinable())
        th.join();
}

/* 
* @brief 对协程进行调度, 销毁运行结束的协程, 处理协程IO事件
*        这里使用了一个与用户线程交互的任务队列和调度线程独有的任务列表
*        解决了用户线程提交任务和调度线程遍历任务调度的竞争问题
*/
void netco_pool::sched_worker::running(void)
{
    auto callback = [this]() {
        netio_task t;
        std::list<netio_task> task_list;

        while (!pool->terminated)
        {
            {
                std::unique_lock<std::mutex> lock(*m_cond_lock);

                /* 0. 从任务队列中取任务放到任务列表中, 头插: 新任务优先调度 */
                while (!task_que->empty())
                {
                    task_que->dequeue(t);
                    task_list.emplace_front(t);
                }

                /* 1. 如果任务列表和任务队列均为空, 没有可调度的协程, 等待新任务 */
                if (task_list.empty())
                {
                    m_cond->wait(lock);
                    continue;
                }
            }

            /* 2. 轮循调度 */
            rr_sched(task_list);
        }
    };

    th = std::make_shared<std::thread>(callback);
}

/* @brief 轮循调度协程 */
void netco_pool::sched_worker::rr_sched(std::list<netio_task>& task_list)
{
    for (auto it = task_list.begin(); \
        ((!pool->terminated) && (it != task_list.end()));)
    {
        if (it->handle_.promise().run_state == CO_RUNNING)
        {
            /* 恢复协程运行, 协程resume恢复后再次挂起或返回时，resume函数返回 */
            it->handle_.resume();

            /* 如果任务需要IO阻塞, 将IO任务交由Epoll监控
                * 在监控过程中, 该协程不会被调度执行, 直到IO事件发生, 协程状态恢复为RUNNING
                */
            if (it->handle_.promise().run_state == CO_IOWAIT)
            {
                /* &*it 取到元素的地址 */
                pool->io_worker->ioevent_add(&(*it), it->handle_.promise().events);
            }
            /* 如果协程结束, 则销毁 */
            else if (it->handle_.done())
            {
                /* 设置协程在结束时挂起, 因为下面还要使用
                    * 如果结束时不挂起, 则resume返回后handle就已经销毁, 后面不能再使用
                    * 设置了结束时挂起, 需要手动销毁协程: 调用 destroy()
                    */
                it->handle_.destroy();
                task_list.erase(it++);  /* 传递给erase一个副本, 自身自增 */
                tasknum--;
                continue;
            }
            else
            {
                /* 协程既未返回co_return, 也未挂起, 但resume结束了 */
                LOG_FATAL << "internal error : coroutine stop but not return or suspend" << std::endl;
            }
        }

        /* 为实现遍历中删除节点, 不在for语句中写自增 */
        it++;
    }
}

/* @brief 等待线程结束 */
void netco_pool::sched_worker::stop(void)
{
    m_cond->notify_all();
    if (th->joinable())
        th->join();
}

} } // namespace