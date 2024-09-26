// References
// [1] https://github.com/smasherprog/screen_capture_lite/blob/master/include/internal/SCCommon.h

#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

// Clock Type Selection
// This selects the appropriate clock type based on whether high_resolution_clock is steady.
// If it is, it uses high_resolution_clock; otherwise, it falls back to steady_clock.

// Member Variables
// m_duration: Stores the duration for the timer in microseconds.
// m_deadline: Stores the time point when the timer will expire.

// Constructor
// This initializes the timer with a given duration, converting it to microseconds and setting the deadline.

// Timer::start()
// Resets the deadline to the current time plus the duration.

// Timer::wait()
// Makes the current thread sleep until the deadline is reached.

class Timer
{
    using Clock = std::conditional<std::chrono::high_resolution_clock::is_steady,
                                   std::chrono::high_resolution_clock,
                                   std::chrono::steady_clock>::type;

    std::chrono::microseconds m_duration;
    Clock::time_point         m_deadline;

public:
    template <typename Rep, typename Period>
    Timer(const std::chrono::duration<Rep, Period> &duration)
        : m_duration(std::chrono::duration_cast<std::chrono::microseconds>(duration))
        , m_deadline(Clock::now() + m_duration)
    {
    }
    void start() { m_deadline = Clock::now() + m_duration; }
    void wait()
    {
        const auto now = Clock::now();
        if (now < m_deadline)
            std::this_thread::sleep_for(m_deadline - now);
    }
    std::chrono::microseconds duration() const { return m_duration; }
};

// A function to simulate a time-consuming computation
void time_consuming_operation()
{
    std::cout << std::format("{} Starting a time-consuming computation...\n", std::chrono::system_clock::now());

    // Simulate a heavy computation by calculating the sum of a large vector
    for (int i = 0; i < 100; ++i) {
        // Simulate a heavy computation by calculating the sum of a large vector
        std::vector<int> large(1000000, 1); // Vector with 1,000,000 elements, all set to 1
        [[maybe_unused]] int64_t sum = std::accumulate(large.begin(), large.end(), 0LL);
    }

    std::cout << std::format("{} Time-consuming computation completed.\n", std::chrono::system_clock::now());
}

int main()
{
    // Create a Timer object with a duration of 2 seconds
    Timer timer(std::chrono::seconds(2));

    std::cout << std::format("{} Timer started for 2 seconds...\n", std::chrono::system_clock::now());

    // Start the timer
    timer.start();

    // Perform the time-consuming operation
    time_consuming_operation();

    // Wait for the timer to expire
    timer.wait();

    std::cout << std::format("{} Timer expired.\n", std::chrono::system_clock::now());

    return 0;
}