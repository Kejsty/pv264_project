#pragma once
#include <memory>
#include <atomic>
#include <cassert>
#include <mutex>
#include <iostream>
#include <unistd.h>
#include <deque>

namespace lock {
namespace sharedPtr {

template <typename T>
struct Queue
{

	struct node {
		node(T value) : _value(value), _next(nullptr) {}

		T _value;
		std::unique_ptr<node> _next;
	};

	Queue() : head(nullptr), tail(nullptr) {}

	bool push(T value) {
		auto toInsert = std::make_unique<node>(std::move(value));

		std::lock_guard<std::mutex> lock(action);
		if (!head) {
			head = move(toInsert);
			tail = head.get();
			return true;
		}

		tail->_next = std::move(toInsert);
		tail = tail->_next.get();
		return true;
	}

	bool pop(T &out) {
		std::lock_guard<std::mutex> lock(action);

		if (!head)
			return false;
		out = head->_value;

		head = std::move(head->_next);

		if (!head)
			tail = nullptr;
		return true;
	}
	
private:
	std::unique_ptr<node> head;
	node *tail;
	std::mutex action;
};

} //namespace sharedPtr
} //namespace lockfree

namespace lock {
namespace wrapper {

template <typename T>
struct Queue {

	bool push(T value) {
		std::lock_guard<std::mutex> lock(action);
		_queue.push_back(value);
		return true;
	}

	bool pop(T &out) {
		std::lock_guard<std::mutex> lock(action);
		if (_queue.empty())
			return false;
		
		out = _queue.front();
		_queue.pop_front();
		return true;
	}

private:
	std::deque<T> _queue;
	std::mutex action;
};

} //namespace wrapper
} //namespace lock