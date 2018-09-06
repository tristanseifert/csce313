#include "Ackerman.h"
#include "BuddyAllocator.h"

int main(int argc, char ** argv) {
  // TODO: read this from the command line with getopt()
  int basic_block_size = 128, memory_length = 4 * 1024 * 1024;

  // create memory manager
  BuddyAllocator *allocator = new BuddyAllocator(basic_block_size, memory_length);

  // test memory manager
  Ackerman *am = new Ackerman();
  am->test(allocator); // this is the full-fledged test.

  // destroy memory manager
  delete allocator;
}
