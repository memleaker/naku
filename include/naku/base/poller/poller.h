#ifndef AMANI_POLLER_H
#define AMANI_POLLER_H

#include <cstdint>
#include <functional>

namespace naku { namespace base {

/* @brief 抽象类poller */
class poller
{
public:
	poller() : callback(nullptr) {}

	void set_callback(std::function<void(void*)> _c) { callback = _c; }

	virtual int ioevent_add(int fd, uint32_t, void *pridata) = 0;
	virtual int ioevent_del(int) = 0;
	virtual int ioevent_handle(void) = 0;
	/* 
	 * 虽然析构函数可以是纯虚函数, 但也要提供实现
	 * 因此直接写成虚函数, 而非纯虚函数
	 */
    virtual ~poller() {};
protected:
	std::function<void(void*)> callback;
};

} } // namespace

#endif