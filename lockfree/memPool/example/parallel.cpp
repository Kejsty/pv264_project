#include "../queue.h"
#include <cassert>
#include <thread>  
#include <iostream>

//Setting input parameters
#define ITER 10000
#define THREADS 10

using Atomic_int = std::atomic<int>;
Atomic_int producerCount(0);
Atomic_int consumerCount(0);

lockfree::memPool::Queue<int> queue;


void producerFn() {
    for (int i = 0; i != ITER; ++i) {
        int value = ++producerCount;
        while(!queue.push(value)) {};
    }
}

void consumerFn() {
	for (int i = 0; i < ITER; ++i) {
        int value;
		while (!queue.pop(value)) {}
		++consumerCount;
	}
}

int main() {
    using namespace std;

    thread producers[THREADS];
    for (int i = 0; i != THREADS; ++i)
        producers[i]= thread(producerFn);

    thread consumers[THREADS];
    for (int i = 0; i != THREADS; ++i)
       consumers[i]= thread(consumerFn);

    for (int i = 0; i != THREADS; ++i)
        producers[i].join();

     for (int i = 0; i != THREADS; ++i)
        consumers[i].join();

    cout << "produced " << producerCount << " objects." << endl;
    cout << "consumed " << consumerCount << " objects." << endl;

    return 0;
}

