## Assignment : Lock-free data structures

Parallel programming is commonly considered as hard and not intuitive compared
with basic sequential programs. Even with use of locks and mutexes is
challenging to develop safe, eg. without deadlocks or livelocks, parallel
program.

Lock free data structures are structures designed for parallel programs. They
are mostly preferred over structures with locks for their better concurrency
and scalability, thanks to reducing blocking or waiting procedures.

The new C++11 atomic library standard throws another wrench into the works,
challenging the way most people express lock-free algorithms. Thanks to
operations as std::atomic_compare_exchange(CAS) and specialized atomic
operations for shared_ptr, we are able to design out own lock-free
structure.

The goal of this project will be to implement simple lock-free queue using linked-list.
I would like to test it not only with shared_ptr
(eg. counting pointers ) but also with my own implemented memory-pool.

### How to compile

```
mkdir build
cd build
ccmake ..
make
```

### Parts of project 

#### directory benchmarks

This directory contains benchmarks for my implementation.

I compare 2 implementations with **lock** (wrapper over a deque and queue as a linked list), and two **lockfree** implementations (with shared pointers and with a memory pool allocator).

Runnable binary: queue\_benchmarks

#### lockfree

This directory contains two lockfree implementations of queues. They differ only in work with **memory**.
One works with shared pointers and atomic operations over them, and the second holds own _memory pool_.

Runnable binaries: queue\_memPool\_basic, queue\_memPool\_test, queue\_memPool\_parallel, queue\_sharedPtr\_basic and queue\_sharedPtr\_parallel

