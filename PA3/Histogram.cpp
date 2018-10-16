#include <iostream>
#include <cstring>
#include <iomanip>

#include <mutex>

#include "Histogram.h"

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
}


void Histogram::update(std::string request, std::string response) {
	// update data
	{
		std::lock_guard<std::mutex> guard(this->lock);

		int person_index = this->map[request];
		this->hist[person_index][stoi(response) / 10]++;
	}
}

void Histogram::print() {
	std::cout << std::setw(10) << std::right << "Range";

	// print names
	for(int j = 0; j < 3; j++) {
		std::lock_guard<std::mutex> guard(this->lock);
		std::cout << std::setw(15) << std::right << this->names[j];
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
				std::lock_guard<std::mutex> guard(this->lock);

				std::cout << std::setw(15) << std::right << this->hist[j][i];
				sum[j] += this->hist[j][i];
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
