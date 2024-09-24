#ifndef NAKU_NAKU_H
#define NAKU_NAKU_H

#include <memory>

#include "base/copool/copool.h"

namespace naku {

static inline void co_init(void)
{
    naku::base::netco_pool::get_instance().init();
}

static inline void co_shutdown(void)
{
    naku::base::netco_pool::get_instance().shutdown();
}

template <typename F, typename... Args>
static inline void co_run(F &&f, Args &&...args)
{
    naku::base::netco_pool::get_instance().submit(f, args...);
}

}

#endif