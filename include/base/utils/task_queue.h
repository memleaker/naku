#ifndef NAKU_TASK_QUEUE_H
#define NAKU_TASK_QUEUE_H

#include <mutex>
#include <queue>

namespace naku { namespace base {

template <typename T>
class task_queue
{
private:
    std::queue<T> m_queue;
    std::mutex    m_mutex;
public:
    bool empty()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    int size()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    void enqueue(const T &t)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.emplace(t);
    }
    bool dequeue(T &t)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;

        t = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }
};

} } // namespace

#endif