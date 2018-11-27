#ifndef SHMREQUESTCHANNEL_H
#define SHMREQUESTCHANNEL_H

#include <string>
#include <atomic>

#include <sys/types.h>
#include <sys/ipc.h>

#include <pthread.h>

#include "RequestChannel.h"

typedef struct shm_msg shm_msg_t;
typedef struct shm_header shm_header_t;

class KernelSemaphore;

class SHMRequestChannel : public RequestChannel {
  friend struct shm_msg;
  friend struct shm_header;

  public:
    SHMRequestChannel(const std::string name, const Side side);
    virtual ~SHMRequestChannel();

  public:
    virtual std::string cread();
    virtual int cwrite(std::string msg);

  public:
    /// maximum number of messages a shared memory segment can hold
    static const size_t kMaxMessages = 32;
    /// maximum size for a SHM message
    static const size_t kMaxMsgSize = 256;

    /// magic value in SHM regions
    static const uint64_t kHeaderMagic = 0xDEADBEEFCAFEBABE;

  private:
    void initSegment(int shmId, shm_header_t *header);
    void deallocSegment(int shmId, shm_header_t *header);

    void lockSegment(shm_header_t *header);
    void unlockSegment(shm_header_t *header);

  private:
    /// key for shared memory region (client write)
    key_t keyClient;
    /// identifier for the memory region (client write)
    int shmIdClient;
    /// semaphore protecting writes to the client region
    KernelSemaphore *semaphoreClient;

    /// key for shared memory region (server write)
    key_t keyServer;
    /// identifier for the message queue (server write)
    int shmIdServer;
    /// semaphore protecting writes to the server
    KernelSemaphore *semaphoreServer;

  private:
    /// start of shared memory region (client write)
    void *shmRegionClient;
    /// header of shared memory region (client write)
    shm_header_t *headerClient;

    /// start of shared memory region (server write)
    void *shmRegionServer;
    /// header of shared memory region (server write)
    shm_header_t *headerServer;
};



/**
 * A single shared memory message.
 */
typedef struct shm_msg {
  /// set if this message is valid
  std::atomic_bool valid;

  /// length of message data
  std::atomic_uint payloadLen;
  /// actual message data
  char payload[SHMRequestChannel::kMaxMsgSize];
} shm_msg_t;

/**
 * Header for a shared memory segment.
 */
typedef struct shm_header {
  /// magic value
  uint64_t magic;
  /// reference count; when zero, shm region is deleted
  std::atomic_uint refCount;

  /// mutex protecting access to the messages array
  pthread_mutex_t messagesLock;

  /// signalled when there are messages available
  pthread_cond_t messagesNotEmpty;
  /// signalled when the message buffer is not full
  pthread_cond_t messagesNotFull;

  /// number of valid entries in the array
  std::atomic_uint numMessages;
  /// actual message array
  shm_msg_t messages[SHMRequestChannel::kMaxMessages];
} shm_header_t;


#endif
