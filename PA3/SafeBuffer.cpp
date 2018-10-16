#include "SafeBuffer.h"

#include <mutex>
#include <string>
#include <queue>

SafeBuffer::SafeBuffer() {

}

SafeBuffer::~SafeBuffer() {

}

int SafeBuffer::size() {
	// get size with lock guard
	{
		std::lock_guard<std::mutex> guard(this->lock);
    return this->q.size();
	}
}

void SafeBuffer::push(std::string str) {
	// perform push with a lock guard
	{
		std::lock_guard<std::mutex> guard(this->lock);
		this->q.push(str);
	}
}

std::string SafeBuffer::pop() {
	std::string s;

	// pop the head of the element with a lock guard
	{
		std::lock_guard<std::mutex> guard(this->lock);

		s = q.front();
		q.pop();
	}

	return s;
}
