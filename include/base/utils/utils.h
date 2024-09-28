#ifndef NAKU_UTILTS_H
#define NAKU_UTILTS_H

#include <vector>
#include <iostream>
#include <cassert>

#include <unistd.h>
#include <sys/time.h>

class utils
{
public:
    static const long max_threads = 200;

public:
    static long cpu_num(void)
    {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }

    static long thread_num(void)
    {
        auto cpu = cpu_num();
        if (cpu == -1)
            return -1;

        cpu *= 2;
        if (cpu <= 0 || cpu > max_threads)
            return max_threads;
    
        return cpu;
    }
};

class posit_num
{
public:
    posit_num(std::size_t n = 0) : num(n) {}

public:
    bool operator>(const posit_num& w) const {return this->num > w.num;}
    bool operator<(const posit_num& w) const {return this->num < w.num;}
    bool operator>=(const posit_num& w) const {return this->num >= w.num;}
    bool operator<=(const posit_num& w) const {return this->num <= w.num;}
    bool operator==(const posit_num& w) const {return this->num == w.num;}
    bool operator!=(const posit_num& w) const {return this->num != w.num;}

    std::size_t operator*(void) const {return num;}

    posit_num operator++() {assert(num!=(size_t)-1); return ++num;}
    posit_num operator--() {assert(num != 0); return --num;}
    posit_num operator++(int) {assert(num!=(size_t)-1); return num++;}
    posit_num operator--(int) {assert(num != 0); return num--;}

private:
    std::size_t num;
};

#endif