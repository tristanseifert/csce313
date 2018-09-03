/*
    File: my_allocator.cpp
*/
#include "BuddyAllocator.h"

#include <iostream>

/**
 * Initialize the memory allocator.
 */
BuddyAllocator::BuddyAllocator(size_t _basicBlockSize, size_t _totalSize) :
	basicBlockSz(_basicBlockSize), totalMemSz(_totalSize) {

}

/**
 * Cleans up the memory allocator.
 */
BuddyAllocator::~BuddyAllocator () {
	// release memory area
	if(this->mem) {
		::free(mem);

		this->mem = nullptr;
	}
}



char *BuddyAllocator::alloc(size_t length) {
  return new char[length];
}

int BuddyAllocator::free(char *block) {
  delete block;
  return 0;
}


 
void BuddyAllocator::debug() {

}
