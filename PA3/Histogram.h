#ifndef Histogram_h
#define Histogram_h

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

class Histogram {
	public:
		Histogram();
		void update(std::string, std::string); 		// updates the histogram
		void print();						// prints the histogram

	private:
		int hist [3][10];					// histogram for each person with 10 bins each
		std::unordered_map<std::string, int> map;  	// person name to index mapping
		std::vector<std::string> names; 				// names of the 3 persons

		/// used as a 'giant' lock for any of the private members
		std::mutex lock;
};

#endif
