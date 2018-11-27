#include "SHMRequestChannel.h"

#include "KernelSemaphore.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <pthread.h>

/// log message send/receives
#define LOG_MSG                     0
/// log creation/dealloc
#define LOG_LIFECYCLE               0

/**
 * Initializes the request channel.
 */
SHMRequestChannel::SHMRequestChannel(const std::string _name, const Side _side) : RequestChannel(_name, _side) {
  // create/attach client shared memory section
  std::string clientFilename = this->getFileName(CLIENT_SIDE);
  this->createFile(clientFilename);

  this->keyClient = this->keyForFile(clientFilename);

  this->shmIdClient = shmget(this->keyClient, sizeof(shm_header_t), IPC_CREAT | 0666);

  this->shmRegionClient = shmat(this->shmIdClient, nullptr, 0);

  if(this->shmRegionClient == ((void *) -1)) {
    this->handleError("shmat (client) failed");
  }

#if LOG_LIFECYCLE
  std::cout << "mapped client end at " << std::hex << this->shmRegionClient << " with key " << this->keyClient << std::dec << std::endl;
#endif

  this->headerClient = static_cast<shm_header_t *>(this->shmRegionClient);

  this->initSegment(this->shmIdClient, this->headerClient);

  this->semaphoreClient = new KernelSemaphore(_name + "_client", kMaxMessages);



  // create/attach server shared memory section
  std::string serverFilename = this->getFileName(SERVER_SIDE);
  this->createFile(serverFilename);

  this->keyServer = this->keyForFile(serverFilename);

  this->shmIdServer = shmget(this->keyServer, sizeof(shm_header_t), IPC_CREAT | 0666);

  this->shmRegionServer = shmat(this->shmIdServer, nullptr, 0);

  if(this->shmRegionServer == ((void *) -1)) {
    this->handleError("shmat (server) failed");
  }

#if LOG_LIFECYCLE
  std::cout << "mapped server end at " << std::hex << this->shmRegionServer << " with key " << this->keyServer << std::dec << std::endl;
#endif

  this->headerServer = static_cast<shm_header_t *>(this->shmRegionServer);

  this->initSegment(this->shmIdServer, this->headerServer);
}

/**
 * Cleans up the shared memory request channel.
 */
SHMRequestChannel::~SHMRequestChannel() {
  // if refcount for client segment is zero, deallocate segment
  if(--this->headerClient->refCount == 0) {
#if LOG_LIFECYCLE
    std::cout << "refcount is 0 for client segment, dealloc" << std::endl << std::flush;
#endif

    this->deallocSegment(this->shmIdClient, this->headerClient);
  }

  // if refcount for server segment is zero, deallocate it
  if(--this->headerServer->refCount == 0) {
#if LOG_LIFECYCLE
    std::cout << "refcount is 0 for server segment, dealloc" << std::endl << std::flush;
#endif

    this->deallocSegment(this->shmIdServer, this->headerServer);
  }
}



/**
 * Initializes a shared memory segment.
 */
void SHMRequestChannel::initSegment(int shmId, shm_header_t *header) {
  int err;

  pthread_mutexattr_t mutexAttr;
  pthread_condattr_t condAttr;

  // set up magic values
  if(header->magic != kHeaderMagic) {
    header->magic = kHeaderMagic;
    header->refCount = 1;

    // set up mutex attributes
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    // allocate mutex
    err = pthread_mutex_init(&header->messagesLock, &mutexAttr);

    if(err != 0) {
      this->handleError("pthread_mutex_init failed");
    }


    // allocate "queue not full" condition
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);

    err = pthread_cond_init(&header->messagesNotFull, &condAttr);

    if(err != 0) {
      this->handleError("pthread_cond_init - messagesNotFull failed");
    }

    // allocate "queue not empty" condition
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);

    err = pthread_cond_init(&header->messagesNotEmpty, &condAttr);

    if(err != 0) {
      this->handleError("pthread_cond_init - messagesNotEmpty failed");
    }
  }
}

/**
 * Deallocates the shared memory region.
 */
void SHMRequestChannel::deallocSegment(int shmId, shm_header_t *header) {
  int err = 0;
  struct shmid_ds buf;

  // make sure the refcount is zero
  if(header->refCount != 0) {
    throw std::runtime_error("refcount must be zero");
  }

  // delete the segment
  err = shmctl(shmId, IPC_RMID, &buf);

  if(err == -1) {
    this->handleError("shmctl (IPC_RMID) failed");
  }

  // destroy condition variables and mutex (no error checking, we do not care)
  pthread_cond_destroy(&header->messagesNotFull);
  pthread_cond_destroy(&header->messagesNotEmpty);

  pthread_mutex_destroy(&header->messagesLock);
}



/**
 * Attempts to lock a shared memory segment.
 */
int SHMRequestChannel::lockSegment(shm_header_t *header, bool fatal) {
  int err = pthread_mutex_lock(&header->messagesLock);
  if(err != 0 && fatal) {
    this->handleError("pthread_mutex_lock failed");
  }

  return err;
}

/**
 * Attempts to unlock a shared memory segment.
 */
int SHMRequestChannel::unlockSegment(shm_header_t *header, bool fatal) {
  int err = pthread_mutex_unlock(&header->messagesLock);
  if(err != 0 && fatal) {
    this->handleError("pthread_mutex_unlock failed");
  }

  return err;
}



/**
 * Reads a message from the channel.
 */
std::string SHMRequestChannel::cread(void) {
  int err;
  std::string result;

  // get the right segment (if client, get server write segment)
  shm_header_t *header = (this->side == SERVER_SIDE) ? this->headerServer : this->headerClient;

  // ok, acquire the lock
  this->lockSegment(header);

  // wait on the "messages not empty" condition
  while(header->numMessages == 0) {
    err = pthread_cond_wait(&header->messagesNotEmpty, &header->messagesLock);

    if(err != 0) {
      this->handleError("pthread_cond_wait - messagesNotEmpty failed");
      goto done;
    }
  }

  // pop a valid message off
  for(int i = 0; i < kMaxMessages; i++) {
    shm_msg_t *msg = &header->messages[i];

    // is this message valid?
    if(msg->valid) {
      // create a string
      result = std::string(msg->payload);

      // mark it as invalid again and clear it
      msg->valid = 0;
      // memset(msg->payload, 0, sizeof(msg->payload));
    }
  }

  // decrement counter
  header->numMessages--;

#if LOG_MSG
  std::cout << getpid() << " - read message '" << result << "' from shm region " << std::hex << header << std::dec << std::endl;
#endif

  // unlock mutex and clean up
done: ;
  this->unlockSegment(header);

  // wake up any threads waiting on the not full condition
  err = pthread_cond_broadcast(&header->messagesNotFull);

  if(err != 0) {
    this->handleError("pthread_cond_signal - messagesNotFull failed");
  }

  return result;
}

/**
 * Writes a message to the channel.
 */
int SHMRequestChannel::cwrite(std::string _msg) {
  int bytesWritten = 0, err;

  // get payload and validate its size
  const char *payload = _msg.c_str();
  size_t payloadLen = strlen(payload) + 1;

  if(payloadLen > kMaxMsgSize) {
    throw std::invalid_argument("message is too big");
  }

  // get the right segment (if client, get client write segment)
  shm_header_t *header = (this->side == CLIENT_SIDE) ? this->headerServer : this->headerClient;

  // ok, acquire the lock
  this->lockSegment(header);

  // wait on the "messages not full" condition
  while(header->numMessages > kMaxMessages) {
    // wait on condition
    err = pthread_cond_wait(&header->messagesNotFull, &header->messagesLock);

    if(err != 0) {
      this->handleError("pthread_cond_wait - messagesNotFull failed");
      goto done;
    }
  }

  // find an empty element to copy the data into
  for(int i = 0; i < kMaxMessages; i++) {
    shm_msg_t *msg = &header->messages[i];

    // if this message is not valid, copy our data into it
    if(msg->valid == false) {
      strncpy(msg->payload, payload, kMaxMsgSize);

      msg->valid = true;
      msg->payloadLen = payloadLen;
    }
  }

  // increment counter
  header->numMessages++;

#if LOG_MSG
  std::cout << getpid() << " - wrote message '" << _msg << "' to shm region " << std::hex << header << std::dec << std::endl;
#endif

  // unlock mutex and clean up
done: ;
  this->unlockSegment(header);

  // wake up any threads/processes waiting on the not empty condition
  err = pthread_cond_broadcast(&header->messagesNotEmpty);

  if(err != 0) {
    this->handleError("pthread_cond_signal - messagesNotEmpty failed");
  }

  return bytesWritten;
}
