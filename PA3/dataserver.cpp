#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "reqchannel.h"
#include <pthread.h>
using namespace std;


int nchannels = 0;
void* handle_process_loop (void* _channel);

/**
 * Creates a new thread that handles requests.
 */
void process_newchannel(RequestChannel *_channel) {
	nchannels++;

	// get the name for the new channel, create it and send it to parent
	std::string new_channel_name = "data" + to_string(nchannels) + "_";
	_channel->cwrite(new_channel_name);

	RequestChannel * data_channel = new RequestChannel(new_channel_name, RequestChannel::SERVER_SIDE);

	// spawn a thread for this
	pthread_t thread_id;
	if(pthread_create(&thread_id, NULL, handle_process_loop, data_channel) < 0) {
		EXITONERROR("");
	}
}

/**
 * Handles a request received on the command pipe.
 */
void process_request(RequestChannel *_channel, std::string _request) {
	// hello command?
	if(_request.compare(0, 5, "hello") == 0) {
		_channel->cwrite("hello to you too");
	}
	// get data
	else if(_request.compare(0, 4, "data") == 0) {
		usleep(1000 + (rand() % 5000));
		_channel->cwrite(to_string(rand() % 100));
	}
	// new channel?
	else if(_request.compare(0, 10, "newchannel") == 0) {
		process_newchannel(_channel);
	}
	// unknown request
	else {
		_channel->cwrite("unknown request");
		abort();
	}
}

void* handle_process_loop(void* _channel) {
	RequestChannel *channel = (RequestChannel *) _channel;
	for(;;) {
		// read command
		std::string request = channel->cread();

		// if quit, exit this thread
		if(request.compare("quit") == 0) {
			// destroy the channel
			// delete channel;

			// TODO: use pthread_exit instead?
			break;
		}

		// otherwise, process command normally
		process_request(channel, request);
	}
}


/**
 * Main function: this can take a single argument, -w, that indicates the number
 * of worker threads to spawn.
 */
int main(int argc, char *argv[]) {
	RequestChannel control_channel("control", RequestChannel::SERVER_SIDE);
	handle_process_loop(&control_channel);
}
