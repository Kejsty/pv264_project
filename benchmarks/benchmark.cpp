#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <atomic>
#include <chrono>

#include "queue_lock.h"
#include "../lockfree/memPool/queue.h"
#include "../lockfree/sharedPtr/queue.h"

using Atomic_int = std::atomic<int>;
using millisec = std::chrono::duration<double, std::milli>;

template <typename Queue,
          int Iter = 1000,
          int ProducersNumber = 2,
          int ConsumerNumber = 2,
          int Repeat = 100>
struct Run {
private:
    std::atomic<bool> isDone;
    Atomic_int producerCount;
    Atomic_int consumerCount;
    std::chrono::microseconds time;
    std::string type;

    Queue queue;
public:

    Run(std::string runType) : isDone(false), 
                                   producerCount(0),
                                   consumerCount(0),
                                   time(0),
                                   type(std::move(runType)),
                                   queue() {}


    void producerFn(void) {
        for (int i = 0; i != Iter; ++i) {
            int value = ++producerCount;
            while(!queue.push(value)) {};
        }
    }


    void consumerFn(void) {
        int val;
        while (!isDone) {
            if (queue.pop(val)) {
                ++consumerCount;
            }
        }

        while (queue.pop(val)) {
            ++consumerCount;
        }

    }

    int iteration() {
        using namespace std;
        producerCount = 0;
        consumerCount = 0;
        
        thread producers[ProducersNumber];
        thread consumers[ConsumerNumber];
        isDone = false;

        auto begin = std::chrono::steady_clock::now();

        for (int i = 0; i != ProducersNumber; ++i) {
            producers[i]= thread(&Run::producerFn, this);
        }

        for (int i = 0; i != ConsumerNumber; ++i) {
            consumers[i]= thread(&Run::consumerFn, this);
        }

        for (int i = 0; i != ProducersNumber; ++i) {
            producers[i].join();
        }

        isDone = true;

        for (int i = 0; i != ConsumerNumber; ++i) {
            consumers[i].join();
        }

        auto end = std::chrono::steady_clock::now();

        if (producerCount != ProducersNumber * Iter) {
            std::cerr << "FAIL: number of produced differs from expected" << std::endl;
            return 1;
        }

        if (consumerCount != producerCount) {
            std::cerr << "FAIL:  number of poped is not equal to number of pushed" << std::endl;
            return 1;
        }

        time += std::chrono::duration_cast< std::chrono::microseconds >(end - begin);
        return 0;
    }

    void run() {
        for (int i = 0; i < Repeat; ++i) {
            if (iteration() == 1)
                return;
        }
        std::cout << "Type: " << type << " Result: [ P: " << ProducersNumber;
        std::cout <<  " C: " << ConsumerNumber << " In: " << Iter * ProducersNumber;
        std::cout <<  " Time: " << time.count() / Repeat << " microseconds]" << std::endl;
    }
};

int main() {
    Run<lock::wrapper::Queue<int>> withLock("lock DequeueWrapper");
    withLock.run();
    Run<lock::sharedPtr::Queue<int>> sharedPtrLock("lock SharedPtr");
    sharedPtrLock.run();
    Run<lockfree::sharedPtr::Queue<int>> lockfreeSharedPtr("lockfree SharedPtr");
    lockfreeSharedPtr.run();
    Run<lockfree::memPool::Queue<int, 131072>> memPool("lockfree MemPool");
    memPool.run();
}