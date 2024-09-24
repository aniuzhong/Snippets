// References
// [1] https://gist.github.com/thelinked/6997598
// [2] https://en.cppreference.com/w/cpp/atomic/memory_order

#include <cassert>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

template<typename T>
class LockingQueue
{
public:
    LockingQueue() = default;
    LockingQueue(const LockingQueue&) = delete;
    LockingQueue& operator=(const LockingQueue&) = delete;
    ~LockingQueue() = default;

public:
    void push(const T& data)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(data);
        }
        m_cond.notify_one();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;

        value = m_queue.front();
        m_queue.pop();
        return true;
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())
            m_cond.wait(lock);

        value = m_queue.front();
        m_queue.pop();
    }

    bool try_wait_and_pop(T& value, int milli)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty()) {
            m_cond.wait_for(lock, std::chrono::milliseconds(milli));
            if (m_queue.empty())
                return false;
        }

        value = m_queue.front();
        m_queue.pop();
        return true;
    }

private:
    std::queue<T>           m_queue;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cond;
};

void producer(LockingQueue<std::string>& queue)
{
    std::string data = "Hello";
    queue.push(data);
}

void consumer(LockingQueue<std::string>& queue)
{
    std::string data;
    queue.wait_and_pop(data);
    assert(data == "Hello");
}

int main()
{
    LockingQueue<std::string> queue;

    std::jthread t1(producer, std::ref(queue));
    std::jthread t2(consumer, std::ref(queue));

    return 0;
}