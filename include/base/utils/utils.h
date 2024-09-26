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

private:
    template<typename T>
    void heapify(std::vector<T> arr, int64_t start, int64_t end)
    {
        int64_t father {start};
        int64_t lchild {father*2+1};

        while (lchild <= end)
        {
            if (lchild+1 <= end && (arr[lchild] < arr[lchild+1])) 
                ++lchild;
            
            if (arr[father] > arr[lchild]) 
                return;
            
            swap(arr[father], arr[lchild]);
            father = lchild;
            lchild = father*2+1;
        }
    }

public:
    template<typename T>
    static T& heap_sort(std::vector<T>& arr)
    {
        int64_t len{arr.size()};

        for (int64_t i = (len-1)/2; i >= 0; --i)
        {
            heapify<T>(arr, i, len-1);
        }

        for (int64_t i = len-1; i > 0; --i)
        {
            swap(arr[0], arr[i]);

            heapify<T>(arr, 0, i-1);
        }
    }
};

class posit_num
{
public:
    posit_num(std::size_t n = 0) : num(n) {}

public:
    bool operator>(posit_num& w)  {return this->num > w.num;}
    bool operator<(posit_num& w)  {return this->num < w.num;}
    bool operator>=(posit_num& w) {return this->num >= w.num;}
    bool operator<=(posit_num& w) {return this->num <= w.num;}
    bool operator==(posit_num& w) {return this->num == w.num;}
    bool operator!=(posit_num& w) {return this->num != w.num;}

    std::size_t operator*(void) {return num;}

    posit_num operator++() {assert(num!=(size_t)-1); return ++num;}
    posit_num operator--() {assert(num != 0); return --num;}
    posit_num operator++(int) {assert(num!=(size_t)-1); return num++;}
    posit_num operator--(int) {assert(num != 0); return num--;}

private:
    std::size_t num;
};

#endif