#include "SHMRequestChannel.h"

#include <iostream>
#include <string>
#include <stdexcept>
#include <functional>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/**
 * Initializes the request channel.
 */
SHMRequestChannel::SHMRequestChannel(const std::string _name, const Side _side) : RequestChannel(_name, _side) {
  // create key for the SHM region
  std::hash<std::string> hash_fn;

  this->key = (key_t) (hash_fn(_name) & 0xFFFFFFFF);
  std::cout << "key for channel '" << _name << "': "
    << std::hex << this->key << std::endl;

  // attempt to get a handle to the shared region
  this->shm_id = shmget(this->key, kSegmentSize, IPC_CREAT | 0660);

  if(this->shm_id == -1) {
    perror("shmget:");
    exit(-1);
  }

  // try to map it
  this->shmRegion = shmat(this->shm_id, nullptr, 0);

  if(this->shmRegion == ((void *) -1)) {
    perror("shmat:");
    exit(-1);
  }

  std::cout << "mapped at " << std::hex << this->shmRegion << std::endl;

  // initialize header if needed
  this->header = static_cast<shm_header_t *>(this->shmRegion);

  if(this->header->magic != kHeaderMagic) {
    this->header->magic = kHeaderMagic;
    this->header->refCount = 1;
  }
}

/**
 * Cleans up the shared memory request channel.
 */
SHMRequestChannel::~SHMRequestChannel() {
  // if refcount is zero, deallocate segment
  if(this->header->refCount-- == 0) {
    std::cout << "refcount is 0, dealloc" << std::endl << std::flush;
    this->deallocSegment();
  }

  // TODO: need to do more stuff here?
}



/**
 * Allocates the shared memory segment.
 */
void SHMRequestChannel::allocSegment(void) {

}

/**
 * Deallocates the shared memory region.
 */
void SHMRequestChannel::deallocSegment(void) {
  int err = 0;
  struct shmid_ds buf;

  // make sure the refcount is zero
  if(this->header->refCount != 0) {
    throw std::runtime_error("refcount must be zero");
  }

  // delete the segment
  err = shmctl(this->shm_id, IPC_RMID, &buf);

  if(err == -1) {
    perror("shmctl:");
    exit(-1);
  }
}



/**
 * Reads a message from the channel.
 */
std::string SHMRequestChannel::cread(void) {
  // throw std::runtime_error("unimplemented");
  return "";
}

/**
 * Writes a message to the channel.
 */
int SHMRequestChannel::cwrite(std::string msg) {
  // throw std::runtime_error("unimplemented");
  return 0;
}
