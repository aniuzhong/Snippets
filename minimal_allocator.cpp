// References
// [1] https://en.cppreference.com/w/cpp/container/vector/reserve
// [2] https://en.cppreference.com/w/cpp/named_req/Allocator

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>
#include <limits>

// [1]
template<class T>
struct NAlloc
{
    typedef T value_type;

    NAlloc() = default;

    template<class U>
    NAlloc(const NAlloc<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n)
    {
        n *= sizeof(T);
        T* p = static_cast<T*>(::operator new(n));
        std::cout << "allocating " << n << " bytes @ " << p << '\n';
        return p;
    }

    void deallocate(T* p, std::size_t n) noexcept
    {
        std::cout << "deallocating " << n * sizeof *p << " bytes @ " << p << "\n\n";
        ::operator delete(p);
    }
};

template<class T, class U>
bool operator==(const NAlloc<T>&, const NAlloc<U>&) { return true; }

template<class T, class U>
bool operator!=(const NAlloc<T>&, const NAlloc<U>&) { return false; }

// [2]
template<class T>
struct Mallocator
{
    typedef T value_type;

    Mallocator() = default;

    template<class U>
    constexpr Mallocator(const Mallocator <U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n)
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();

        if (auto p = static_cast<T*>(std::malloc(n * sizeof(T))))
        {
            report(p, n);
            return p;
        }

        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept
    {
        report(p, n, 0);
        std::free(p);
    }

private:
    void report(T* p, std::size_t n, bool alloc = true) const
    {
        std::cout << (alloc ? "Alloc: " : "Dealloc: ") << sizeof(T) * n
                  << " bytes at " << std::hex << std::showbase
                  << reinterpret_cast<void*>(p) << std::dec << '\n';
    }
};

template<class T, class U>
bool operator==(const Mallocator <T>&, const Mallocator <U>&) { return true; }

template<class T, class U>
bool operator!=(const Mallocator <T>&, const Mallocator <U>&) { return false; }

int main()
{
    constexpr int max_elements = 32;
 
    std::cout << "using reserve: \n";
    {
        std::vector<int, Mallocator<int>> v1;
        v1.reserve(max_elements); // reserves at least max_elements * sizeof(int) bytes

        for (int n = 0; n < max_elements; ++n)
            v1.push_back(n);
    }
 
    std::cout << "not using reserve: \n";
    {
        std::vector<int, Mallocator<int>> v1;

        for (int n = 0; n < max_elements; ++n)
        {
            if (v1.size() == v1.capacity())
                std::cout << "size() == capacity() == " << v1.size() << '\n';
            v1.push_back(n);
        }
    }

    return 0;
}