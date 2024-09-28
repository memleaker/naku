#ifndef AMANI_EPOLLER_H
#define AMANI_EPOLLER_H

#include <iostream>
#include <cstddef>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <memory>

#include <unistd.h>
#include <sys/epoll.h>

#include <naku/base/poller/poller.h>
#include <naku/base/logger/logger.h>

namespace naku { namespace base {

/* @brief epoller 使用Epoll监控IO事件 */
class epoller : public poller
{
public:
	/* @brief 创建epoll和销毁 */
    epoller() : epoll_fd(epoll_create(1))
	{
		if (epoll_fd == -1) {
			LOG_FATAL << "Create epoll instance failed : " << strerror(errno) << std::endl;
		}
	}
	virtual ~epoller() noexcept override { ::close(epoll_fd); }

	/* @brief 添加IO事件监控 */
	virtual int ioevent_add(int fd, uint32_t events, void *pridata) override;

	/* @brief 当关闭fd时, 会自动从epoll中移除, 因此该函数不常调用 */ 
	virtual int ioevent_del(int fd) override;

	/* @brief 监控IO事件, 并设置协程运行状态 */
    virtual int ioevent_handle(void) override;

private:
    int epoll_fd;
};

} } // namespace

#endif
