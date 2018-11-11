#include "BoundedBuffer.h"

#include <iostream>
#include <string>
#include <queue>

#include <pthread.h>

/**
 * The argument `_cap` is the maximum capacity of the buffer.
 */
BoundedBuffer::BoundedBuffer(int _cap) : maxCapacity(_cap) {
  int err;

  // allocate mutex for queue
  err = pthread_mutex_init(&this->queueLock, nullptr);

  if(err != 0) {
    // perror("pthread_mutex_init");
    std::cout << "pthread_mutex_init: " << err << std::endl;
  }

  // allocate "queue not full" condition
  err = pthread_cond_init(&this->queueNotFull, nullptr);

  if(err != 0) {
    // perror("pthread_cond_init - queueNotFull");
    std::cout << "pthread_cond_init - queueNotFull: " << err << std::endl;
  }

  // allocate "queue not empty" condition
  err = pthread_cond_init(&this->queueNotEmpty, nullptr);

  if(err != 0) {
    // perror("pthread_cond_init - queueNotEmpty");
    std::cout << "pthread_cond_init - queueNotEmpty: " << err << std::endl;
  }
}

/**
 * Destroys any resources we allocated.
 */
BoundedBuffer::~BoundedBuffer() {
  int err;

  // destroy "queue not full" condition
  err = pthread_cond_destroy(&this->queueNotFull);

  if(err != 0) {
    // perror("pthread_cond_destroy - queueNotFull");
    std::cout << "pthread_cond_destroy - queueNotFull: " << err << std::endl;
  }

  // destroy "queue not empty" condition
  err = pthread_cond_destroy(&this->queueNotEmpty);

  if(err != 0) {
    // perror("pthread_cond_destroy - queueNotEmpty");
    std::cout << "pthread_cond_destroy - queueNotEmpty: " << err << std::endl;
  }

  // destroy the queue mutex last (prevents EBUSY)
  err = pthread_mutex_destroy(&this->queueLock);

  if(err != 0) {
    // perror("pthread_mutex_destroy");
    std::cout << "pthread_mutex_destroy: " << err << std::endl;
  }
}



/**
 * Returns the size of the queue.
 */
int BoundedBuffer::size() {
  // acquire the mutex for the queue first
  this->lockQueue();

  // now, get the size
	std::size_t size = q.size();

  // and unlock the lock again
  this->unlockQueue();

  return size;
}

/**
 * Pushes an element to the back of the queue. If the queue is already at the
 * maximum capacity, this call blocks until some data is popped.
 */
void BoundedBuffer::push(std::string str) {
  int err;

  // acquire the queue lock
  this->lockQueue();

  // wait on the "queue not full" condition
  // we access size on the queue directly bc we own the queue lock
  while(this->q.size() >= this->maxCapacity) {
    // wait on condition; lock is acquired again if successful
    err = pthread_cond_wait(&this->queueNotFull, &this->queueLock);

    if(err != 0) {
      perror("pthread_cond_wait - queueNotFull");
    }
  }

  // push the element, then release lock
	q.push(str);

  this->unlockQueue();

  // wake up any threads waiting on the not empty condition
  // we don't use pthread_cond_broadcast since we only added one item to queue
  err = pthread_cond_broadcast(&this->queueNotEmpty);

  if(err != 0) {
    perror("pthread_cond_signal - queueNotEmpty");
  }
}

/**
 * Pops the first element off the queue. If there is nothing in the queue, this
 * call will block.
 */
std::string BoundedBuffer::pop() {
  int err;

  // acquire the queue lock
  this->lockQueue();

  // wait on the "queue not empty" condition
  // we access size on the queue directly bc we own the queue lock
  while(this->q.size() == 0) {
    // wait on condition; lock is acquired again if successful
    err = pthread_cond_wait(&this->queueNotEmpty, &this->queueLock);

    if(err != 0) {
      std::cout << "pthread_cond_wait - queueNotFull: " << err << std::endl;
    }
  }

  // pop the head element off the queue, then unlock queue
  std::string s = q.front();
	q.pop();

  this->unlockQueue();

  // wake up any threads waiting on the not full condition
  // we don't use pthread_cond_broadcast since we only added one item to queue
  err = pthread_cond_broadcast(&this->queueNotFull);

  if(err != 0) {
    perror("pthread_cond_signal - queueNotFull");
  }

	return s;
}
