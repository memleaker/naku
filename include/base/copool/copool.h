#ifndef NAKU_COROUTINE_POOL_H
#define NAKU_COROUTINE_POOL_H

#include <cstdlib>
#include <mutex>
#include <list>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <coroutine>
#include <functional>
#include <iostream>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include "base/poller/epoller.h"
#include "base/copool/thpool.h"
#include "base/copool/netio_task.h"
#include "base/copool/task_queue.h"

namespace naku { namespace base {

/* Singleton Pattern */
class netco_pool
{
private:
	/* @brief 默认工作线程数量为2, 实际情况会多创建一个线程用于监控IO事件 */
	netco_pool(unsigned int threadnum = 2) : 
		terminated(true), sche_threads(threadnum), total_threads(threadnum+1), thpool(threadnum+1),
		task_queues(std::vector<task_queue<netio_task>>(threadnum)) {}

    /* @brief 禁用拷贝和移动 */
    netco_pool(const netco_pool &) = delete;
    netco_pool(netco_pool &&) noexcept = delete;
    netco_pool &operator=(const netco_pool &) = delete;
    netco_pool &operator=(netco_pool &&) noexcept = delete;

public:
	static netco_pool& get_instance()
	{
		static netco_pool pool;
		return pool;
	}

	/* @brief Member Functions */
public:
	/* @brief 初始化协程池 */
	void init(void)
	{
		/* 0. 标记协程池运行状态 */
		terminated = false;

		/* 1. 创建epoll线程 */
		poll = std::make_shared<epoller>();

		/* 2. 启动线程池, 等待任务 */
		thpool.init();

		/* 3. 向线程池的工作线程提交工作任务
		      第一个线程为监控IO事件线程，其它为调度用户协程的进程
		 */
		thpool.submit([this] { this->poll_run(); });
		for (unsigned int i = 0; i < sche_threads; i++)
		{
			thpool.submit([i, this] {this->co_run(task_queues[i]);});
		}
	}

	/* @brief 关闭协程池 */
	void shutdown(void)
	{
		terminated = true;
		thpool.shutdown();
	}

	/* 
	 * @brief 向协程池提交任务
	 * @param f	待执行任务的函数名
	 * @param args 待执行任务的参数
	 */
   	template <typename F, typename... Args>
	void submit(F &&f, Args &&...args)
	{
		static int thid = 0;

		/* 
		 1. submit 时, 直接运行协程, 由于协程设置启动时挂起
		    即可在这里取到协程的handle
		 2. 取到handle, 将返回的netio_task存储起来, 方便对协程进行控制(恢复)
		 3. 将线程按照任务数量排序，将任务放到任务量最少的线程上去
		    当最少的线程任务量达到
		*/
		netio_task task_handle = f(args...);
		task_queues[thid++ % sche_threads].enqueue(task_handle);
	}

	/* 
	 * @brief 对协程IO事件进行监控, 发生IO事件时修改协程状态
	 */
	void poll_run(void)
	{
		while (!terminated)
		{
			if (poll->ioevent_handle() == -1)
			{
				LOG_ERROR << "poll thread exit!!!" << std::endl;
				return ;
			}
		}
	}

	/* 
	 * @brief 对协程进行调度, 销毁运行结束的协程, 处理协程IO事件
     *        这里使用了一个与用户线程交互的任务队列和调度线程独有的任务列表
	 *        解决了用户线程提交任务和调度线程遍历任务调度的竞争问题
	 * @param task_que 保存用户submit的协程任务
	 * @param poll IO多路复用对象，用于监控IO事件
	 */
	void co_run(task_queue<netio_task> &task_que)
	{
		netio_task t;
		std::list<netio_task> task_list;

		while (!terminated)
		{
			/* 0. 从任务队列中取任务放到任务列表中, 头插: 新任务优先调度 */
			while (task_que.dequeue(t))
			{
				task_list.emplace_front(t);
			}

			/* 1. 等待任务列表不为空 */
			if (task_list.empty())
			{
				usleep(1);
				continue;
			}

			/* 2. 从任务列表中取出任务执行 */
			for (auto it = task_list.begin(); \
				((!terminated) && (it != task_list.end()));)
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
						poll->ioevent_add(&(*it), it->handle_.promise().events);
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
	}

private:
	bool terminated;

	/* 调度线程数量和总线程数量 */
	unsigned int sche_threads;
	unsigned int total_threads;

	/* 线程池 */
	thread_pool thpool;

	/* poller */
	std::shared_ptr<poller> poll;

	/* 一个线程一个任务队列, 用于暂存用户提交的协程任务
	   一个线程一个，避免了多线程使用一个队列发生竞争的情况
	 */
	std::vector<task_queue<netio_task>> task_queues;
};

} } // namespace

#endif // NAKU_COROUTINE_POOL_H