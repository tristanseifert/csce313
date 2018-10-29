#include "BoundedBuffer.h"
#include <string>
#include <queue>
using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) {
	
}

BoundedBuffer::~BoundedBuffer() {
	
}

int BoundedBuffer::size() {
	return q.size();
}

void BoundedBuffer::push(string str) {
	/*
	Is this function thread-safe??? Does this automatically wait for the pop() to make room 
	when the buffer if full to capacity???
	*/
	q.push (str);
}

string BoundedBuffer::pop() {
	/*
	Is this function thread-safe??? Does this automatically wait for the push() to make data available???
	*/
	string s = q.front();
	q.pop();
	return s;
}
