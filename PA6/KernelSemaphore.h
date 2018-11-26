#ifndef KERNELSEMAPHORE_H
#define KERNELSEMAPHORE_H

#include <string>

#include <semaphore.h>

class KernelSemaphore {
  public:
    KernelSemaphore(std::string name, int val);
    ~KernelSemaphore();

  public:
    void P(void); // acquire lock
    void V(void); // release lock

  private:
    void handleError(std::string);

  private:
    std::string name;

    sem_t *handle;
};

#endif
