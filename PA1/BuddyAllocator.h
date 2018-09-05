/*
    File: my_allocator.h

    Original Author: R.Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 08/02/08

    Modified:

	Resources:
		See BuddyAllocator dot cpp
*/

#ifndef _BuddyAllocator_h_
#define _BuddyAllocator_h_

#include <iostream>

struct BlockHeader {
	// is this block allocated?
	size_t		allocated:1;
	// how big is this block?
	size_t		size:56;

	// next free block
	BlockHeader *nextFree;
};


class BuddyAllocator {
	private:
		// smallest block we can allocate
		size_t basicBlockSz;
		// total amount of allocated memory
		size_t totalMemSz;

		// amount of memory we've satisfied allocations for
		size_t allocationsSatisfied = 0;

		// free list for each size
		BlockHeader **freeList = nullptr;
		size_t freeListLength;

		// allocated memory area
		void *mem = nullptr;

	private:
		/// checks whether the free list contains the given block
		bool freeListContainsBlock(BlockHeader *block);
		/// inserts a block into the free list
		bool insertBlockIntoFreeList(BlockHeader *block);
		/// removes a block from the free list
		bool removeBlockFromFreeList(BlockHeader *block);

		/* private function you are required to implement
		 this will allow you and us to do unit test */
		BlockHeader* getbuddy(BlockHeader *addr);
		// given a block address, this function returns the address of its buddy

		bool isvalid(BlockHeader *addr);
		// Is the memory starting at addr is a valid block
		// This is used to verify whether the a computed block address is actually correct

		bool arebuddies(BlockHeader* block1, BlockHeader* block2);
		// checks whether the two blocks are buddies are not

		BlockHeader* merge(BlockHeader* block1, BlockHeader* block2);
		// this function merges the two blocks returns the beginning address of the merged block
		// note that either block1 can be to the left of block2, or the other way around

		BlockHeader* split(BlockHeader* block);
		// splits the given block by putting a new header halfway through the block
		// also, the original header needs to be corrected

		// given a block size, return the free list index
		size_t freeListIndexForSize(size_t size) {
			int basicPowerOf2 = __builtin_ctzll(this->basicBlockSz);
			int sizePowerOf2 = __builtin_ctzll(size);

			return (sizePowerOf2 - basicPowerOf2);
		}


	public:
		BuddyAllocator(size_t basicBlockSize, size_t totalSize);
		/* This initializes the memory allocator and makes a portion of
		   ’_total_memory_length’ bytes available. The allocator uses a ’_basic_block_size’ as
		   its minimal unit of allocation. The function returns the amount of
		   memory made available to the allocator. If an error occurred,
		   it returns 0.
		*/

		~BuddyAllocator();
		/* Destructor that returns any allocated memory back to the operating system.
		   There should not be any memory leakage (i.e., memory staying allocated).
		*/

		void *alloc(size_t length);
		/* Allocate _length number of bytes of free memory and returns the
			address of the allocated portion. Returns 0 when out of memory. */

		int free(void *block);
		/* Frees the section of physical memory previously allocated
		   using ’my_malloc’. Returns 0 if everything ok. */

		void debug();
		/* Mainly used for debugging purposes and running short test cases */
		/* This function should print how many free blocks of each size belong to the allocator
		at that point. The output format should be the following (assuming basic block size = 128 bytes):

		128: 5
		256: 0
		512: 3
		1024: 0
		....
		....
		 which means that at point, the allocator has 5 128 byte blocks, 3 512 byte blocks and so on.*/
};

#endif
