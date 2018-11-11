#ifndef Histogram_h
#define Histogram_h

#include <string>
#include <vector>
#include <unordered_map>

#include <pthread.h>

class Histogram {
	public:
		Histogram();
    ~Histogram();

		void update(std::string, std::string); 		// updates the histogram
		void print();						// prints the histogram

    void markBinAsComplete(std::string);
    void waitForCompletion(void);

	private:
		int hist[3][10];					// histogram for each person with 10 bins each
		std::unordered_map<std::string, int> map;  	// person name to index mapping
		std::vector<std::string> names; 				// names of the 3 persons

    /// completion status
    bool completion[3];
    /// condition variable to signal change in completion state
    pthread_cond_t completionCondition;

		/// used as a 'giant' lock for any of the private members
    pthread_mutex_t histLock;

  private:
    /**
     * ANDs together the completion state.
     */
    bool isComplete() {
      bool status = true;

      for(int i = 0; i < 3; i++) {
        status &= this->completion[i];
      }

      return status;
    }

  private:
    /**
     * Attempts to lock the histogram mutex.
     */
    inline void lockHistogram(void) {
      int err = pthread_mutex_lock(&this->histLock);
      if(err != 0) {
        std::cout << "pthread_mutex_lock: " << err << std::endl;
      }
    }

    /**
     * Attempts to unlock the histogram mutex.
     */
    inline void unlockHistogram(void) {
      int err = pthread_mutex_unlock(&this->histLock);
      if(err != 0) {
        std::cout << "pthread_mutex_unlock: " << err << std::endl;
      }
    }
};

#endif
