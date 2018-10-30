#include <iostream>
#include <cstring>
#include <iomanip>

#include <pthread.h>

#include "Histogram.h"

/**
 * Initializes the histogram.
 */
Histogram::Histogram() {
	for (int i=0; i < 3; i++){
		memset(hist[i], 0, 10 * sizeof(int));
	}

	map["data John Smith"] = 0;
	map["data Jane Smith"] = 1;
	map["data Joe Smith"] = 2;

	names.push_back("John Smith");
	names.push_back("Jane Smith");
	names.push_back("Joe Smith");

  // allocate lock
  int err;

  // allocate mutex for queue
  err = pthread_mutex_init(&this->histLock, nullptr);

  if(err != 0) {
    std::cout << "pthread_mutex_init: " << err << std::endl;
  }

  // initialize completion condition
  err = pthread_cond_init(&this->completionCondition, nullptr);

  if(err != 0) {
    std::cout << "pthread_cond_init - completionCondition: " << err << std::endl;
  }
}

/**
 * Deallocates the lock.
 */
Histogram::~Histogram() {
  int err;

  // destroy the condition
  err = pthread_cond_destroy(&this->completionCondition);

  if(err != 0) {
    std::cout << "pthread_cond_destroy - completionCondition: " << err << std::endl;
  }

  // destroy lock
  err = pthread_mutex_destroy(&this->histLock);

  if(err != 0) {
    std::cout << "pthread_mutex_destroy: " << err << std::endl;
  }
}


void Histogram::update(std::string request, std::string response) {
	// update data
	this->lockHistogram();

	int index = this->map[request];
	this->hist[index][stoi(response) / 10]++;

  this->unlockHistogram();
}

void Histogram::print() {
	std::cout << std::setw(10) << std::right << "Range";

	// print names
	for(int j = 0; j < 3; j++) {
    this->lockHistogram();

		std::cout << std::setw(15) << std::right << this->names[j];

    this->unlockHistogram();
	}

	std::cout << std::endl;
	std::cout << "----------------------------------------------------------" << std::endl;

	int sum[3];
	memset(sum, 0, 3 * sizeof (int));

	// sum each of them
	for(int i = 0; i < 10; i++){
		std::string range = "[" + std::to_string(i*10) + " - " + std::to_string((i+1)*10 - 1) + "]:";
		std::cout << std::setw(10) << std::right << range;

		for(int j = 0; j < 3; j++) {
			// sum using a lock to protect access to data
			{
        this->lockHistogram();

				std::cout << std::setw(15) << std::right << this->hist[j][i];
				sum[j] += this->hist[j][i];

        this->unlockHistogram();
			}
		}

		std::cout << std::endl;
	}

	// print the sum
	std::cout <<"----------------------------------------------------------" << std::endl;
	std::cout << std::setw(10) << std::right << "Totals:";

	for(int j = 0; j < 3; j++) {
		std::cout << std::setw(15) << std::right << sum[j];
	}

	std::cout << std::endl;
}



/**
 * Marks a particular bin as complete.
 */
void Histogram::markBinAsComplete(std::string request) {
  int err;

  // set the completion flag
	this->lockHistogram();

	int index = this->map[request];
  this->completion[index] = true;

  this->unlockHistogram();

  // then signal anyone waiting on this condition
  err = pthread_cond_broadcast(&this->completionCondition);

  if(err != 0) {
    std::cout << "pthread_cond_broadcast: " << err << std::endl;
  }
}

/**
 * Waits for the histogram to be "complete," e.g. each of the inputs has been
 * marked as finished.
 */
void Histogram::waitForCompletion(void) {
  int err;

  // acquire the queue lock
  this->lockHistogram();

  // wait on the completion condition
  // we access completion directly bc we own the queue lock
  while(this->isComplete() == false) {
    // wait on condition; lock is acquired again if successful
    err = pthread_cond_wait(&this->completionCondition, &this->histLock);

    if(err != 0) {
      std::cout << "pthread_cond_wait - completionCondition: " << err << std::endl;
    }
  }

  // we're good
}
