#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <iostream>
#include <queue>
#include <string>

#include <pthread.h>

class BoundedBuffer {
  public:
    BoundedBuffer(int);
    ~BoundedBuffer();

    bool isEmpty(void) {
      return (this->size() == 0);
    }

    int size();
    void push(std::string);
    std::string pop();

  // data structures
  private:
    std::queue<std::string> q;

  // synchronization helpers
  private:
    int maxCapacity;

    pthread_mutex_t queueLock;

    pthread_cond_t queueNotFull;
    pthread_cond_t queueNotEmpty;

  private:
    /**
     * Attempts to lock the queue mutex.
     */
    inline void lockQueue(void) {
      int err = pthread_mutex_lock(&this->queueLock);
      if(err != 0) {
        // perror("pthread_mutex_lock");
        std::cout << "pthread_mutex_lock: " << err << std::endl;
      }
    }

    /**
     * Attempts to unlock the queue mutex.
     */
    inline void unlockQueue(void) {
      int err = pthread_mutex_unlock(&this->queueLock);
      if(err != 0) {
        // perror("pthread_mutex_unlock");
        std::cout << "pthread_mutex_unlock: " << err << std::endl;
      }
    }
};

#endif /* BoundedBuffer_ */
