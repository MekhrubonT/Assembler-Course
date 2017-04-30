#include <emmintrin.h>
#include <functional>
#include <iostream>
#include <cassert>

void copy_nt_asm(char* dst, char const* src, size_t N) {
    const size_t step = 16;
    auto copy_while = [&dst, &src, &N](const std::function<bool()> &pred) {
        while (pred() && N > 0) {
            *dst = *src;
            dst++;
            src++;
            N--;            
        }
    };
    copy_while([&dst, &step, &N]() {return (size_t) dst % step != 0;});

    while (N >= step) {
        __m128i tmp;
        __asm__ volatile (
            "movdqu     (%1), %0\n"
            "movntdq    %0, (%2)\n"
            : "=x"(tmp)
            : "r"(src), "r"(dst)
        );
        N -= step;
        dst += step;
        src += step;
    }

    copy_while([]() { return true;});
    _mm_sfence();
}

void copy_nt_asm(void* dst, void const* src, size_t N) {
    copy_nt_asm(reinterpret_cast<char*>(dst), reinterpret_cast<char const*>(src), N);
}

int main() {
    srand(time(0));
    for (int test = 0; test < 10000; ++test) {
        int size = rand() % 20000 + 1;
        int a[size];
        int b[size];
        for (auto &x : a) {
            x = rand();
        }
        copy_nt_asm(b, a, sizeof(a));
        for (int i = 0; i < size; ++i) {
            assert(a[i] == b[i]);
        }
    }
}