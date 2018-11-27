#ifndef SHMREQUESTCHANNEL_H
#define SHMREQUESTCHANNEL_H

#include <string>
#include <atomic>

#include <sys/types.h>
#include <sys/ipc.h>

#include "RequestChannel.h"

/**
 * Header for a shared memory segment.
 */
typedef struct {
  /// magic value
  uint64_t magic;
  /// reference count; when zero, shm region is deleted
  std::atomic_uint refCount;
} shm_header_t;

class SHMRequestChannel : public RequestChannel {
  public:
    SHMRequestChannel(const std::string name, const Side side);
    virtual ~SHMRequestChannel();

  public:
    virtual std::string cread();
    virtual int cwrite(std::string msg);

  public:
    /// size of the shared memory segment
    static const size_t kSegmentSize = (1024 * 32);
    /// magic value in SHM regions
    static const uint64_t kHeaderMagic = 0xDEADBEEFCAFEBABE;

  private:
    void allocSegment(void);
    void deallocSegment(void);

  private:
    /// key for shared memory region
    key_t key;
    /// identifier for the memory region (returned by shmget())
    int shmId;

  private:
    /// start of shared memory region
    void *shmRegion;

    /// header of shared memory region
    shm_header_t *header;
};


#endif
