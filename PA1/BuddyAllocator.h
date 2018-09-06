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
	// is this block valid?
	size_t		valid:1;
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
		unsigned freeListContainsBlock(BlockHeader *block, size_t freeListIndex);
		bool freeListContainsBlock(BlockHeader *block);
		/// inserts a block into the free list
		bool insertBlockIntoFreeList(BlockHeader *block);
		/// removes a block from the free list
		bool removeBlockFromFreeList(BlockHeader *block);

		/// given a block address, return the address of its buddy
		BlockHeader* getbuddy(BlockHeader *addr);
		/// checks whether the two blocks are buddies are not
		bool arebuddies(BlockHeader* block1, BlockHeader* block2);
		/// Is the memory starting at addr is a valid block
		bool isvalid(BlockHeader *addr);

		/// this function merges the two blocks returns the beginning address of the merged block
		BlockHeader* merge(BlockHeader* block1, BlockHeader* block2);
		/// splits the given block by putting a new header halfway through the block
		BlockHeader* split(BlockHeader* block);

		/// given a block size, return the free list index
		size_t freeListIndexForSize(size_t size) {
			int basicPowerOf2 = __builtin_ctzll(this->basicBlockSz);
			int sizePowerOf2 = __builtin_ctzll(size);

			return (sizePowerOf2 - basicPowerOf2);
		}

	private:
		// finds which freelist the block is part of
		void debugFindBlockInFreeList(BlockHeader *block);
		// checks whether any blocks have gotten corrupted
		void debugCheckBlockHeaders(void);


	public:
		BuddyAllocator(size_t basicBlockSize, size_t totalSize);
		~BuddyAllocator();

		/// attempts to perform an allocation
		void *alloc(size_t length);
		/// frees the block
		int free(void *block);

		/// prints debug info on the freelist and memory usage
		void debug();
};

#endif
