#ifndef KERNELSEMAPHORE_H
#define KERNELSEMAPHORE_H

class KernelSemaphore {
  public:
    KernelSemaphore(int val);
    ~KernelSemaphore();

  public:
    void P(void); // acquire lock
    void V(void); // release lock

  private:
};

#endif
