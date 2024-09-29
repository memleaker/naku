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
#include <semaphore>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include <naku/base/poller/epoller.h>
#include <naku/base/copool/netio_task.h>
#include <naku/base/utils/task_queue.h>
#include <naku/base/utils/utils.h>

namespace naku { namespace base {

/* @brief 协程池类, 全局唯一实例, 单例模式 */
class netco_pool
{
private:
	/* @brief 默认工作线程数量为2, 实际情况会多创建一个线程用于监控IO事件 */
	netco_pool(unsigned int _nthreads = 2) : terminated(true), nthreads(_nthreads){}

    /* @brief 禁用拷贝和移动 */
    netco_pool(const netco_pool &) = delete;
    netco_pool(netco_pool &&) noexcept = delete;
    netco_pool &operator=(const netco_pool &) = delete;
    netco_pool &operator=(netco_pool &&) noexcept = delete;

	~netco_pool() {this->shutdown();}

public:
	static netco_pool& get_instance()
	{
		static netco_pool pool;
		return pool;
	}

public:
	/* @brief 初始化协程池 */
	void init(void)
	{
		long n;
		
		/* 0. 标记协程池运行状态 */
		terminated = false;

		/* 1. 启动IO监控线程 */
		io_worker = std::make_unique<iomul_worker>(new epoller(), this);
		io_worker->running();

		/* 2. 启动调度线程 */
		if ((n = utils::thread_num()) == -1) {
			// log("Get Core number failed, create n_threads thread")
			n = nthreads;
		}

		for (long i = 0; i < n; i++)
		{
			/* @brief 
			 * 1. 由于sched_workers不支持拷贝, 因此不能接收emplace_back返回值, 也就不能在此处调用running()
			 * 2. running函数中需要使用this指针, 因此需要在emplace_back后再调用running
			 *    否则this指针是局部变量
			 */
			sched_workers.emplace_back(std::move(sched_worker(this)));
		}

		for (auto &w : sched_workers)
		{
			w.running();
		}
	}

	/* @brief 阻塞等待协程池结束 */
	void evloop(void)
	{
		io_worker->stop();

		for (auto &w : sched_workers)
		{
			w.stop();
		}
	}

	/* @brief 关闭协程池 */
	void shutdown(void)
	{
		terminated = true;
		evloop();
	}

	/* 
	 * @brief 向协程池提交任务
	 * @param f	待执行任务的函数名
	 * @param args 待执行任务的参数
	 */
   	template <typename F, typename... Args>
	netio_task submit(F &&f, Args &&...args)
	{
		static std::mutex submit_lock;

		/* 
		 1. submit 时, 直接运行协程, 由于协程设置启动时挂起
		    即可在这里取到协程的handle
		 2. 取到handle, 将返回的netio_task存储起来, 方便对协程进行控制(恢复)
		 3. 使用优先队列(堆实现), 将任务放到任务量最少的线程上去
		*/
		netio_task task_handle = f(args...);
		
		/* @brief lock 用于保护数据结构 sched_workers */
		std::unique_lock<std::mutex> lock(submit_lock);

		/* @brief 获取最小任务数量的worker */
		std::cout << sched_workers[0].taskcount() << std::endl;
		sched_workers[0].submit(task_handle);

		return task_handle;
	}

public:
	/* @brief IO多路复用监控IO事件线程 */
	class iomul_worker
	{
	public:
		iomul_worker(poller *_poller, netco_pool *_pool) : 
			poll(_poller), pool(_pool) {}

		/* @brief 新增IO事件进行监控 */
		int ioevent_add(netio_task *task, int events);

		/* @brief 对协程IO事件进行监控, 发生IO事件时修改协程状态 */
		void running(void);

		/* @brief 等待线程结束 */
		void stop(void);

	private:
		std::unique_ptr<std::thread> th;
		std::unique_ptr<poller> poll;
		netco_pool *pool;
	};

	/* @brief 调度执行协程的线程 */
	class sched_worker
	{
	private:
		sched_worker() = delete;

	public:
		sched_worker(netco_pool *_pool) : 
			tasknum(0), pool(_pool), task_que(std::make_unique<task_queue<netio_task>>()),
			m_cond_lock(std::make_unique<std::mutex>()), m_cond(std::make_unique<std::condition_variable>()) {}

		/* @brief move construct. */
		sched_worker(sched_worker&& w)
		{
			if (this == &w)
				return;

			this->tasknum = w.tasknum;
			this->th = std::move(w.th);
			this->pool = w.pool;
			this->m_cond = std::move(w.m_cond);
			this->m_cond_lock = std::move(w.m_cond_lock);
			this->task_que = std::move(w.task_que);
		}

		sched_worker& operator=(sched_worker&& w)
		{
			if (this == &w)
				return *this;

			this->tasknum = w.tasknum;
			this->th = std::move(w.th);
			this->pool = w.pool;
			this->m_cond = std::move(w.m_cond);
			this->m_cond_lock = std::move(w.m_cond_lock);
			this->task_que = std::move(w.task_que);
			return *this;
		}

		/* 
		* @brief 对协程进行调度, 销毁运行结束的协程, 处理协程IO事件
		*        这里使用了一个与用户线程交互的任务队列和调度线程独有的任务列表
		*        解决了用户线程提交任务和调度线程遍历任务调度的竞争问题
		* @param task_que 保存用户submit的协程任务
		* @param poll IO多路复用对象，用于监控IO事件
		*/
		void running(void);

		/* @brief 轮循调度协程 */
		void rr_sched(std::list<netio_task>& list);

		/* @brief 等待线程结束 */
		void stop(void);

		/* @brief 获取任务数量 */
		std::size_t taskcount(void) {return *tasknum;}

		/* @brief 提交任务 */
		void submit(netio_task task)
		{
			tasknum++;
			task_que->enqueue(task);
			m_cond->notify_one();
		}

	private:
		posit_num tasknum;
		netco_pool *pool;

		/* 
		 * @brief 使用指针是因为mutex和cond不可拷贝不可移动, thread不可拷贝
		 * 使用vector需要实现拷贝或移动构造函数
		 */
		std::unique_ptr<std::thread> th;
		std::unique_ptr<task_queue<netio_task>> task_que;
		std::unique_ptr<std::mutex> m_cond_lock;
		std::unique_ptr<std::condition_variable> m_cond;
	};

private:
	bool terminated;
	unsigned int nthreads;

	std::unique_ptr<iomul_worker> io_worker;
	std::vector<sched_worker> sched_workers;
	std::priority_queue<posit_num, std::vector<posit_num>, std::greater<sched_worker>> prioq;
};

} } // namespace

#endif // NAKU_COROUTINE_POOL_H