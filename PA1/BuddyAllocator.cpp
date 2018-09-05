/*
    File: my_allocator.cpp

	Resources:
	- http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	- https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c
*/
#include "BuddyAllocator.h"

#include <cstring>

#include <stdexcept>

#include <iostream>
#include <iomanip>

/**
 * Rounds a number up to the nearest power of two.
 */
static size_t RoundUpPowerOf2(size_t in) {
	unsigned int v = in;

	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}


/**
 * Initialize the memory allocator.
 */
BuddyAllocator::BuddyAllocator(size_t _basicBlockSize, size_t _totalSize) :
	basicBlockSz(_basicBlockSize), totalMemSz(_totalSize) {
	// round the total allocation size to a power of two
	this->totalMemSz = RoundUpPowerOf2(this->totalMemSz);

	std::cout << "Basic block size:\t" << this->basicBlockSz << std::endl;
	std::cout << "Total memory size:\t" << this->totalMemSz << std::endl;

	// allocate the internal buffer and zero it
	this->mem = malloc(this->totalMemSz);

	if(!this->mem) {
		throw std::runtime_error("couldn't allocate memory");
	}

	memset(this->mem, 0, this->totalMemSz);

	// allocate the freelist
	int basicPowerOf2 = __builtin_ctzll(this->basicBlockSz);
	int totalPowerOf2 = __builtin_ctzll(this->totalMemSz);

	int freeListEntries = (totalPowerOf2 - basicPowerOf2) + 1;

	std::cout << "Free list has " << freeListEntries << " entries" << std::endl;

	size_t freeListSize = sizeof(BlockHeader *) * freeListEntries;

	this->freeList = reinterpret_cast<BlockHeader **>(malloc(freeListSize));
	this->freeListLength = freeListEntries;

	if(!this->freeList) {
		throw std::runtime_error("couldn't allocate free list");
	}

	memset(this->freeList, 0, freeListSize);

	// create the initial block
	BlockHeader *initial = reinterpret_cast<BlockHeader *>(this->mem);

	initial->allocated = 0;
	initial->size = this->totalMemSz;
	initial->nextFree = nullptr;

	// plop it in the free list
	this->freeList[(this->freeListLength - 1)] = initial;

	// print debug shit
	this->debug();
}

/**
 * Cleans up the memory allocator.
 */
BuddyAllocator::~BuddyAllocator() {
	// release memory area
	if(this->mem) {
		::free(this->mem);
	}

	// release freelist
	if(this->freeList) {
		::free(this->freeList);
	}
}


/**
 * Returns the buddy to this block.
 *
 * If this block is of size S, expressed as 2^n, at address A, the buddy is the
 * block at address A ^ (1 << (n - 1))
 */
BlockHeader* BuddyAllocator::getbuddy(BlockHeader *block) {
	// get the size
	size_t blockSize = block->size;

	size_t bit = __builtin_ctzll(blockSize) - 1;

	// flip the bit, thank you sir
	size_t addr = (size_t) block;

	addr ^= (1 << (bit));

	return reinterpret_cast<BlockHeader *>(addr);
}

/*
 * Splits the specified block in half, then returns the first buddy.
 */
BlockHeader* BuddyAllocator::split(BlockHeader* block) {
	// remove this block from the free list
	size_t freeListIndex = freeListIndexForSize(block->size);

	std::cout << "\tsplitting block " << std::hex << block << " at free list idx "
		<< std::dec << freeListIndex << std::endl;

	if(this->freeList[freeListIndex]) {
		// handle the case where it's the first element
		if(this->freeList[freeListIndex] == block) {
			this->freeList[freeListIndex] = block->nextFree;

			std::cout << "\t\tremoved block from free list index " << freeListIndex << std::endl;
		} else {
			// iterate through til we find this block
			BlockHeader *current = this->freeList[freeListIndex];

			while(current) {
				// is the next entry this block?
				if(current->nextFree == block) {
					// if so, change it
					current->nextFree = block->nextFree;

					std::cout << "\t\tremoved block from free list index " << freeListIndex << std::endl;
				}

				// go to the next block
				current = current->nextFree;
			}
		}
	} else {
		// the free list should have an entry, wtf
		throw std::runtime_error("free list is corrupted");
	}

	// update the first block's header
	block->size /= 2;

	// create a new header halfway through
	char *secondBlockPtr = reinterpret_cast<char *>(block);
	secondBlockPtr += block->size;

	BlockHeader *secondBlock = reinterpret_cast<BlockHeader *>(secondBlockPtr);
	memset(secondBlock, 0, sizeof(BlockHeader));

	secondBlock->size = block->size;

	// insert these two new blocks at the head of the free list
	block->nextFree = secondBlock;
	secondBlock->nextFree = this->freeList[(freeListIndex - 1)];

	this->freeList[(freeListIndex - 1)] = block;

	// std::cout << "\tnext block: " << std::hex << block->nextFree << "; second block next is " << secondBlock->nextFree << std::endl;

	// return the original block
	return block;
}



/**
 * Allocates a block.
 */
void *BuddyAllocator::alloc(size_t length) {
	// we need a block that's the requested size, plus the header
	size_t realLength = length + sizeof(BlockHeader);
	realLength = RoundUpPowerOf2(realLength);

	// if it's less than the minimum block size, round it up
	if(realLength < this->basicBlockSz) {
		realLength = this->basicBlockSz;
	}

	std::cout << std::dec;
	std::cout << "requested allocation for " << length << ", finding block size "
		<< realLength << " to satisfy it" << std::endl;

	// is there something in the free list?
	size_t freeListIndex = this->freeListIndexForSize(realLength);

	std::cout << "\tfree list index " << freeListIndex << std::endl;

	if(this->freeList[freeListIndex]) {
		// that was easy, lmfao
		BlockHeader *header = this->freeList[freeListIndex];

		std::cout << "\t✅ satisfied via free list, block is " << std::hex << header
			<< std::dec << std::endl;
		std::cout << "\tupdating free list: next free block is " << std::hex
			<< header->nextFree << std::dec << std::endl;

		// update the free list
		this->freeList[freeListIndex] = header->nextFree;

		// mark the block as allocated
		header->allocated = 1;

		this->debug();

		std::cout << "\treturning block sized " << header->size << std::endl;

		// return the data
		char *dataPtr = reinterpret_cast<char *>(header);
		dataPtr += sizeof(BlockHeader);

		return dataPtr;
	}

	// couldn't find an entry in the free list, so start breaking larger blocks
	for(size_t i = freeListIndex; i < this->freeListLength; i++) {
		// is there a block of this size free?
		if(this->freeList[i]) {
			// break it down into a smaller block
			BlockHeader *split = this->split(this->freeList[i]);

			// if this block is more than one block larger, split it further
			if((i - 1) > freeListIndex) {
				size_t levels = i - freeListIndex;

				for(size_t j = 0; j < (levels - 1); j++) {
					split = this->split(split);
				}
			}

			// we are now done
			split->allocated = 1;

			std::cout << "\t✅ satisfied by splitting, block is " << std::hex << split
				<< std::dec << std::endl;

			this->debug();

			std::cout << "\treturning block sized " << split->size << std::endl;

			char *dataPtr = reinterpret_cast<char *>(split);
			dataPtr += sizeof(BlockHeader);

			return dataPtr;
		}
	}

	// if we reach down here, we couldn't satisfy the request
	this->debug();

	std::cout << "couldn't satisfy allocation request :(" << std::endl;

	return nullptr;
}

/**
 * Deallocates a block.
 */
int BuddyAllocator::free(void *block) {
	// get the block header
	char *headerPtr = static_cast<char *>(block);
	headerPtr -= sizeof(BlockHeader);

	BlockHeader *header = reinterpret_cast<BlockHeader *>(headerPtr);

	// clear the allocated flag
	header->allocated = 0;

	// is this block's buddy free?
	BlockHeader *buddy = this->getbuddy(header);

	if(buddy->allocated == 0) {
		this->merge(header, buddy);
	} else {
		// just put it back in its free list
		size_t freeListIndex = this->freeListIndexForSize(header->size);

		if(this->freeList[freeListIndex]) {
			// iterate through the list and append it at the end
			BlockHeader *current = this->freeList[freeListIndex];

			while(current) {
				// is this block's next entry null?
				if(current->nextFree) {
					current = current->nextFree;
				} else {
					// if so, insert this block
					current->nextFree = header;
					header->nextFree = nullptr;
				}
			}
		} else {
			this->freeList[freeListIndex] = header;
		}
	}

  return 0;
}



void BuddyAllocator::debug() {
	size_t blockSize = this->basicBlockSz;

	// print header
	std::cout << std::setw(8) << "Block Size\tFree Blocks" << std::endl;

	// for each content of the free list, print the number of available free blocks
	for(size_t i = 0; i < this->freeListLength; i++) {
		std::cout << std::setw(8) << blockSize << "\t";

		// count the number of free blocks
		int blocks = 0;
		BlockHeader *header = this->freeList[i];

		while(header != nullptr) {
			blocks++;
			header = header->nextFree;
		}

		std::cout << blocks << std::endl;

		// block size is multiplied by two
		blockSize *= 2;
	}
}
