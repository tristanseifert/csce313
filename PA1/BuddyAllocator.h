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
#include <stdexcept>

// forward declared types
class BuddyAllocator;



/**
 * Memory block header: each allocation has this structure immediately previous
 * to the data pointer returned to the caller.
 *
 * The `nextFree` field is used to build a linked list of free blocks.
 */
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



/**
 * Linked list, used in the block headers. This is kind of an afterthought
 * since the data elements for the list are in the block headers.
 */
class LinkedList {
	public:
		static bool insert(BuddyAllocator *allocator, BlockHeader *block);
		static bool remove(BuddyAllocator *allocator, BlockHeader *block);
};



/**
 * Buddy memory allocator
 */
class BuddyAllocator {
	friend class LinkedList;

	private:
		static const size_t FREE_LIST_MAX = 48;

	private:
		// smallest block we can allocate
		size_t basicBlockSz = 0;
		// total amount of allocated memory
		size_t totalMemSz = 0;

		// amount of memory we've satisfied allocations for
		size_t allocationsSatisfied = 0;

		// free list for each size
		BlockHeader *freeList[BuddyAllocator::FREE_LIST_MAX];
		size_t freeListLength = 0;

		// allocated memory area
		void *mem = nullptr;

	private:
		/// checks whether the free list contains the given block
		unsigned freeListContainsBlock(BlockHeader *block, size_t freeListIndex);
		bool freeListContainsBlock(BlockHeader *block);

		/// inserts a block into the free list
		bool insertBlockIntoFreeList(BlockHeader *block) {
			return LinkedList::insert(this, block);
		}
		/// removes a block from the free list
		bool removeBlockFromFreeList(BlockHeader *block) {
			return LinkedList::remove(this, block);
		}


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

			size_t index = (sizePowerOf2 - basicPowerOf2);

			// make sure its in bounds
			if(index > this->freeListLength) {
				throw std::runtime_error("couldn't convert size to valid free list idx");
			}

			return index;
		}

	private:
		// finds which freelist the block is part of
		void debugFindBlockInFreeList(BlockHeader *block);
		// checks whether any blocks have gotten corrupted
		void debugCheckBlockHeaders(void);


	public:
		BuddyAllocator(size_t basicBlockSize, size_t totalSize);
		virtual ~BuddyAllocator();

		/// attempts to perform an allocation
		void *alloc(size_t length);
		/// frees the block
		int free(void *block);

		/// prints debug info on the freelist and memory usage
		void debug();
};

#endif
