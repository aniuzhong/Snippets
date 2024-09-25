// References
// [1] http://www.cppblog.com/Solstice/archive/2013/01/28/197597.html

#include <atomic>
#include <memory>
#include <thread>

#define USE_ATOMIC 1

class Foo
{
};

#if USE_ATOMIC
std::atomic<std::shared_ptr<Foo>> g;
#else
std::shared_ptr<Foo> g; // race condition [1]
#endif

void ta()
{
    std::shared_ptr<Foo> x = g;
}

void tb()
{
    std::shared_ptr<Foo> n(new Foo); // Foo2
    g = n;
}

int main()
{
    g = std::make_shared<Foo>(); // Foo1

    std::jthread t1(ta);
    std::jthread t2(tb);

    return 0;
}