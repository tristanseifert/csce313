#include "KernelSemaphore.h"

#include <stdexcept>
#include <sstream>

#include <cstdio>

#include <semaphore.h>

/**
 * Creates a kernel semaphore.
 */
KernelSemaphore::KernelSemaphore(std::string _name, int _val) : name(_name) {
  // make sure semaphore value is in range
  if(_val > SEM_VALUE_MAX) {
    throw std::domain_error("initial value must be less than SEM_VALUE_MAX");
  }

  // get name for semaphore
  const char *semName = this->name.c_str();

  // try to create semaphore
  this->handle = sem_open(semName, O_CREAT, 0660, _val);

  if(this->handle == SEM_FAILED) {
    this->handleError("sem_open failed");
  }
}

/**
 * Releases the kernel semaphore.
 */
KernelSemaphore::~KernelSemaphore() {

}



/**
 * Attempts to acquire the semaphore.
 */
void KernelSemaphore::P(void) {
  int err;

  // try to lock semaphore (decrement it)
  err = sem_wait(this->handle);

  if(err != 0) {
    this->handleError("sem_wait failed");
  }
}

/**
 * Attempts to release the semaphore.
 */
void KernelSemaphore::V(void) {
  int err;

  // try to unlock semaphore (increment it)
  err = sem_post(this->handle);

  if(err != 0) {
    this->handleError("sem_post failed");
  }
}



/**
 * Handles a semaphore-related error. This creates an exception with the
 * supplied description, and the error number.
 */
void KernelSemaphore::handleError(std::string description) {
  std::stringstream msg;

  msg << description;
  msg << ": ";
  msg << strerror(errno);

  throw std::runtime_error(msg.str());
}
