/*
    File: my_allocator.cpp

	Resources:
	- http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	- https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c
	- https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
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

	initial->valid = 1;
	initial->allocated = 0;
	initial->size = this->totalMemSz;
	initial->nextFree = nullptr;

	// plop it in the free list
	if(this->insertBlockIntoFreeList(initial) == false) {
		throw std::runtime_error("couldn't insert initial block into free list");
	}

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

	size_t bit = __builtin_ctzll(blockSize);

	// flip the bit, thank you sir
	size_t addr = (size_t) block;

	addr ^= (1 << (bit));

	return reinterpret_cast<BlockHeader *>(addr);
}




/**
 * Checks whether the block exists in the free list for the given size.
 */
unsigned BuddyAllocator::freeListContainsBlock(BlockHeader *block, size_t freeListIndex) {
	// this counts the number of occurrences
	unsigned occurrences = 0;

	// is the free list valid?
	if(this->freeList[freeListIndex]) {
		// check if it's the head of the list
		BlockHeader *current = this->freeList[freeListIndex];

		/*if(current == block) {
			occurrences++;
		}*/

		// iterate through the list
		while(current) {
			// is this the block we're after?
			if(current == block) {
				occurrences++;
			}

			// go to the next one
			current = current->nextFree;
		}
	}

	// if we get down here, we couldn't find it
	return occurrences;
}

/**
 * Checks whether the block exists in the free list.
 */
bool BuddyAllocator::freeListContainsBlock(BlockHeader *block) {
	size_t freeListIndex = freeListIndexForSize(block->size);

	return (this->freeListContainsBlock(block, freeListIndex) > 0);
}

/**
 * Inserts this block into its free list.
 */
bool BuddyAllocator::insertBlockIntoFreeList(BlockHeader *block) {
	// find where this block is in the freelist
	// this->debugFindBlockInFreeList(block);

	// return if it's already in the free list
	if(this->freeListContainsBlock(block)) {
		throw std::runtime_error("attempting to insert block that's already in freelist");
	}

	// get index into the free list
	size_t freeListIndex = freeListIndexForSize(block->size);

	// if the freelist is empty, insert it at the head
	if(this->freeList[freeListIndex] == nullptr) {
		this->freeList[freeListIndex] = block;
		block->nextFree = nullptr;

		return true;
	} else {
		// traverse the free list til the end
		BlockHeader *current = this->freeList[freeListIndex];

		while(current) {
			// is the next entry null?
			if(current->nextFree == nullptr) {
				// if so, insert this block there
				current->nextFree = block;
				// set this block's next pointer to null
				block->nextFree = nullptr;

				return true;
			}

			// go to the next one
			current = current->nextFree;
		}
	}

	// we should never get down here
	return false;
}

/**
 * Removes a block from a free list.
 */
bool BuddyAllocator::removeBlockFromFreeList(BlockHeader *block) {
	// find where this block is in the freelist
	// this->debugFindBlockInFreeList(block);

	// make sure this block is in the free list
	if(!this->freeListContainsBlock(block)) {
		throw std::runtime_error("attempting to remove block that's not in freelist");
	}

	// check the free list
	size_t freeListIndex = freeListIndexForSize(block->size);

	if(this->freeList[freeListIndex]) {
		// handle the case where it's the first element
		if(this->freeList[freeListIndex] == block) {
			this->freeList[freeListIndex] = block->nextFree;

			std::cout << "\t\tremoved block from free list index " << freeListIndex << std::endl;
			return true;
		} else {
			// iterate through til we find this block
			BlockHeader *current = this->freeList[freeListIndex];

			while(current) {
				// is the next entry this block?
				if(current->nextFree == block) {
					// if so, change it
					current->nextFree = block->nextFree;

					std::cout << "\t\tremoved block from free list index " << freeListIndex << std::endl;
					return true;
				}

				// go to the next block
				current = current->nextFree;
			}
		}
	} else {
		// the free list should have an entry, wtf
		throw std::runtime_error("freelist is corrupted");
	}

	// if we get down here, we couldn't remove it, wtf
	return false;
}



/**
 * Merges two adjacent blocks.
 */
BlockHeader* BuddyAllocator::merge(BlockHeader* block1, BlockHeader* block2) {
	// ensure block2 comes after block1
	if(block1 > block2) {
		BlockHeader *first = block2;
		BlockHeader *second = block1;

		block1 = first;
		block2 = second;

		// throw std::invalid_argument("block2 must come after block1, wtf are you doing");
	}

	// remove block2 from the free list
	if(this->removeBlockFromFreeList(block2) == false) {
		throw std::runtime_error("couldn't remove block2 from freelist");
	}
	// remove block1 from the free list
	if(this->removeBlockFromFreeList(block1) == false) {
		throw std::runtime_error("couldn't remove block1 from freelist");
	}

	// cool, block1's size needs to be doubled
	block1->size *= 2;
	block1->valid = 1;

	// clear the contents of the block
	size_t clearSz = block1->size - sizeof(BlockHeader);

	char *dataStart = reinterpret_cast<char *>(block1);
	dataStart += sizeof(BlockHeader);

	memset(dataStart, 0, clearSz);

	// insert block1 into the freelist of the next level
	if(this->insertBlockIntoFreeList(block1) == false) {
		throw std::runtime_error("couldn't insert block1 into freelist");
	}

	// merged the block, yeet
	return block1;
}

/*
 * Splits the specified block in half, then returns the first buddy.
 */
BlockHeader* BuddyAllocator::split(BlockHeader* block) {
	size_t freeListIndex = freeListIndexForSize(block->size);

	std::cout << "\tsplitting block " << std::hex << block << " at free list idx "
		<< std::dec << freeListIndex << std::endl;

	// remove this block from the free list
	if(this->removeBlockFromFreeList(block) == false) {
		throw std::runtime_error("couldn't remove block from freelist");
	}

	// update the first block's header
	block->size /= 2;
	block->allocated = 0;
	block->valid = 1;

	// create a new header halfway through
	char *secondBlockPtr = reinterpret_cast<char *>(block);
	secondBlockPtr += block->size;

	BlockHeader *secondBlock = reinterpret_cast<BlockHeader *>(secondBlockPtr);
	memset(secondBlock, 0, sizeof(BlockHeader));

	// copy the properties from the other header
	secondBlock->size = block->size;
	secondBlock->allocated = block->allocated;
	secondBlock->valid = block->valid;

	// insert these two new blocks at the head of the free list
	if(!this->insertBlockIntoFreeList(block)) {
			throw std::runtime_error("couldn't insert first split block into freelist");
	}

	if(!this->insertBlockIntoFreeList(secondBlock)) {
			throw std::runtime_error("couldn't insert second split block into freelist");
	}

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

		// remove it from the freelist
		if(this->removeBlockFromFreeList(header) == false) {
			throw std::runtime_error("couldn't remove block from freelist");
		}

		// mark the block as allocated
		header->allocated = 1;

		this->debug();

		std::cout << "\treturning block sized " << header->size << " at address "
			<< std::hex << header << std::dec << std::endl;

		// accounting
		this->allocationsSatisfied += header->size;

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

			// remove it from the freelist
			if(this->removeBlockFromFreeList(split) == false) {
				throw std::runtime_error("couldn't remove block from freelist");
			}

			// we are now done
			split->allocated = 1;

			std::cout << "\t✅ satisfied by splitting, block is " << std::hex << split
				<< std::dec << std::endl;

			this->debug();

			std::cout << "\treturning block sized " << split->size << " at address "
				<< std::hex << split << std::dec << std::endl;

			// accounting
			this->allocationsSatisfied += split->size;

			char *dataPtr = reinterpret_cast<char *>(split);
			dataPtr += sizeof(BlockHeader);

			return dataPtr;
		}
	}

	// if we reach down here, we couldn't satisfy the request
	this->debug();

	std::cout << "couldn't satisfy allocation request; allocated "
		<< this->allocationsSatisfied << " bytes of " << this->totalMemSz
		<< std::endl;

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

	std::cout << "deallocating block " << std::hex << block << std::dec
		<< " of size " << header->size << std::endl;

	// subtract this block's size from the amount of memory we've allocated
	this->allocationsSatisfied -= header->size;

	// clear the allocated flag
	header->allocated = 0;

	// is this block's buddy free?
	BlockHeader *buddy = this->getbuddy(header);

	std::cout << "\tbuddy for " << std::hex << header << " is " << buddy
		<< std::dec << std::endl;

	if(buddy->allocated == 0 && false) {
		if(buddy->valid) {
			std::cout << "\tits buddy is unallocated, merging" << std::endl;

			this->merge(header, buddy);
		} else {
			std::cout << "\tits buddy is allocated but not valid (wtf);"
			 	<< "inserting into freelist as is" << std::endl;

			// put the block back in its free list
			if(this->insertBlockIntoFreeList(header) == false) {
				throw std::runtime_error("couldn't insert block into freelist");
			}
		}
	} else {
		std::cout << "\tits buddy is allocated, inserting into freelist as is"
			<< std::endl;

		// put the block back in its free list
		if(this->insertBlockIntoFreeList(header) == false) {
			throw std::runtime_error("couldn't insert block into freelist");
		}
	}

  return 0;
}



/**
 * A debugging check that determines whether the free list is valid.
 */
void BuddyAllocator::debugFindBlockInFreeList(BlockHeader *block) {
	std::cout << "Searching freelist for block " << std::hex << block << std::dec
		<< ": ";

	for(size_t i = 0; i < this->freeListLength; i++) {
		// find how many times the block occurred in this free list
		unsigned occurrences = this->freeListContainsBlock(block, i);

		if(occurrences > 0) {
			std::cout << i << "(" << occurrences << ") ";
		}
	}

	// end the line
	std::cout << std::endl;
}

/**
 * Prints out allocation statistics.
 */
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

	// print how much memory was allocated
	float pctAlloc = (((float) this->allocationsSatisfied) / ((float) this->totalMemSz)) * 100.f;
	std::cout << "Total memory allocated: " << this->allocationsSatisfied
		<< " of " << this->totalMemSz << " bytes (" << pctAlloc
		<< "%)" << std::endl;
}
