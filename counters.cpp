// References
// [1] https://en.cppreference.com/w/cpp/atomic/atomic
// [2] https://en.cppreference.com/w/cpp/atomic/memory_order
// [3] https://en.cppreference.com/w/cpp/language/cv

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

class ThreadsafeCounter
{
public:
    explicit ThreadsafeCounter(int value = 0) : m_value(value) {}

public:
    int get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }
 
    void inc()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_value;
    }

private:
    mutable std::mutex m_mutex; // The "M&M rule": mutable and mutex go together
    int                m_value;
};

std::atomic_int   acnt;
int               cnt;
ThreadsafeCounter sfcnt;

void f()
{
    for (int n = 0; n < 10000; ++n)
    {
        ++cnt;
        acnt.fetch_add(1, std::memory_order_relaxed);
        sfcnt.inc();
    }
}

int main()
{
    {
        std::vector<std::jthread> pool;
        for (int n = 0; n < 10; ++n)
            pool.emplace_back(f);
    }
 
    std::cout << "The atomic counter is " << acnt << '\n'
              << "The non-atomic counter is " << cnt << '\n'
              << "The thread safe counter is " << sfcnt.get() << '\n';
}