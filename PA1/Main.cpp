#include "Ackerman.h"
#include "BuddyAllocator.h"

#include <cstdlib>
#include <unistd.h>

extern "C" char *optarg;
extern "C" int optind;

int main(int argc, char **argv) {
  // TODO: read this from the command line with getopt()
  int basic_block_size = 128, memory_length = 512 * 1024;

  // parse command line parameters
  int c;
  while((c = getopt(argc, argv, "b:s:")) != -1) {
    switch(c) {
      // basic block size
      case 'b':
        basic_block_size = strtol(optarg, nullptr, 10);
        break;

      // total memory size, in bytes
      case 's':
        memory_length = strtol(optarg, nullptr, 10);
        break;

      // error
      case '?':
        std::cout << "usage: " << argv[0] << " [-b blocksize] "
          << "[-s memsize]" << std::endl << std::endl;
        std::cout << "PARAMETERS" << std::endl;
        std::cout << "-b: Basic block size, in bytes. Defaults to 128" << std::endl;
        std::cout << "-s: Size of the memory region to allocate, in bytes. Defaults to 524288 (512KB)" << std::endl;


        exit(-1);
        break;

      // shouldn't get there
      default:
        break;
    }
  }


  // create memory manager
  BuddyAllocator *allocator = new BuddyAllocator(basic_block_size, memory_length);

  // test memory manager
  Ackerman *am = new Ackerman();
  am->test(allocator); // this is the full-fledged test.

  // destroy memory manager
  delete allocator;
}
