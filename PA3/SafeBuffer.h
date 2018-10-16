#ifndef SafeBuffer_h
#define SafeBuffer_h

#include <mutex>
#include <queue>
#include <string>

class SafeBuffer {
	public:
		SafeBuffer();
		~SafeBuffer();

	public:
		int size();
		void push(std::string str);
		std::string pop();

	private:
		std::mutex lock;
		std::queue<std::string> q;
};

#endif /* SafeBuffer_ */
