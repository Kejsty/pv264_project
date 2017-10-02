#include "../queue.h"
#include <cassert>
#include <vector>


int main() {
	lockfree::memPool::Queue<size_t> queue;
	
	std::vector<size_t> v{1,2,3,4,5,6,7,8,9,10};

	for (size_t i = 0; i < v.size(); ++i) {
		queue.push(i);
	}
	
	for (size_t i = 0; i < v.size(); ++i) {
		size_t value = 0;
		bool result = queue.pop(value);
		assert(result);
		assert(value == i);
	}
}