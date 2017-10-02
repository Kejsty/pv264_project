#define CATCH_CONFIG_MAIN
#include <thread>
#include <set>
#define HOLDSIZE 1
#include "../lockfree/memPool/queue.h"
#include "catch.hpp"




TEST_CASE("valid after construct") {
	lockfree::memPool::Queue<int, 512> queue;
	REQUIRE(queue.empty());
	REQUIRE(queue.used() == 1); //for sentinel
	REQUIRE(queue.available() == 511);
}

TEST_CASE("push and pop") {
	lockfree::memPool::Queue<int, 512> queue;
	queue.push(5);
	REQUIRE(!queue.empty());
	REQUIRE(queue.used() == 2); //for sentinel
	REQUIRE(queue.available() == 510);

	int result;
	queue.pop(result);
	REQUIRE(result == 5);

	REQUIRE(queue.empty());
	REQUIRE(queue.used() == 1); //for sentinel
	REQUIRE(queue.available() == 511);
}

void producerFn(lockfree::memPool::Queue<size_t,512> *queue, size_t from, size_t to) {
    for (size_t i = from; i < to; ++i) {
        while(!queue->push(i)) {};
    }
}


TEST_CASE("no element lost in push") {
  const size_t repeat =  400;
	lockfree::memPool::Queue<size_t, 512> queue;
    std::thread producers[3];
    producers[0]= std::thread(producerFn, &queue, 0, repeat/4);
    producers[1]= std::thread(producerFn, &queue, repeat/4, repeat/2);
    producers[2]= std::thread(producerFn, &queue, repeat/2,repeat);

  	for (int i = 0; i < 3; ++i) {
        producers[i].join();
   	}

   	std::set<size_t> content;


    for (size_t i = 0; i < repeat; ++i) {
      content.insert(i);
    }

   	size_t current;
   	while (queue.pop(current)) {
        content.erase(current);
    }


	REQUIRE(queue.empty());
	REQUIRE(queue.used() == 1); //for sentinel
	REQUIRE(queue.available() == 511);

	REQUIRE(content.size() == 0);
	REQUIRE(content.empty());
}


void consumerFn(lockfree::memPool::Queue<size_t,512> *queue,
                          std::set<size_t> *results) {
    	size_t value;
      while(queue->pop(value)) {
        results->insert(value);
      }
      
}

TEST_CASE("no element lost in pop") {
  const size_t repeat =  400;
	lockfree::memPool::Queue<size_t, 512> queue;
	for (size_t i = 0; i < repeat; ++i) {
		queue.push(i);
	}

    std::thread consumers[3];
    std::set<size_t> sets[3];
    consumers[0]= std::thread(consumerFn, &queue, &sets[0]);
    consumers[1]= std::thread(consumerFn, &queue, &sets[1]);
    consumers[2]= std::thread(consumerFn, &queue, &sets[2]);


  	for (int i = 0; i < 3; ++i) {
        consumers[i].join();
   	}

    std::set<size_t> content;

   	for (size_t i = 0; i < repeat; ++i) {
      content.insert(i);
    }

    for (int i = 0; i < 3; ++i) {
      for (auto &item : sets[i]){
        content.erase(item);
      }
    }
   
	REQUIRE(queue.empty());
	REQUIRE(queue.used() == 1); //for sentinel
	REQUIRE(queue.available() == 511);

	REQUIRE(content.size() == 0);
	REQUIRE(content.empty());
}

