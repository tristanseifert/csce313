/*
    Based on original assignment by: Dr. R. Bettati, PhD
    Department of Computer Science
    Texas A&M University
    Date  : 2013/01/31
 */


#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>

#include <sys/time.h>
#include <cassert>
#include <assert.h>

#include <cmath>
#include <numeric>
#include <algorithm>

#include <list>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "reqchannel.h"
#include "SafeBuffer.h"
#include "Histogram.h"
using namespace std;

/**
 * Data type passed to the threads to populate the request buffer.
 */
typedef struct {
	/// buffer into which to insert data
	SafeBuffer *buf;

	/// person for whom to get info
	std::string name;
	/// how many requests to make
	size_t numRequests;
} buf_populate_ctx_t;



/**
 * Entry point for the request thread.
 */
void *PopulateThreadEntry(void *_ctx) {
    buf_populate_ctx_t *ctx = static_cast<buf_populate_ctx_t *>(_ctx);

    // create the command to push
    std::string command = "data " + ctx->name;

    // push it N times
    for(size_t i = 0; i < ctx->numRequests; i++) {
        ctx->buf->push(command);
    }

    // done
    return nullptr;
}



/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION */
/*--------------------------------------------------------------------------*/
int main(int argc, char * argv[]) {
    int err;
    int n = 100; //default number of requests per "patient"
    int w = 1; //default number of worker threads
    int opt = 0;
    while ((opt = getopt(argc, argv, "n:w:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg); //This won't do a whole lot until you fill in the worker thread function
                break;
			}
    }

    // remove fork() lol

    cout << "n == " << n << endl;
    cout << "w == " << w << endl;

    cout << "CLIENT STARTED:" << endl;

    // this is the buffer and histogram used by everything
	SafeBuffer request_buffer;
	Histogram hist;

    // create the threads to push requests
    const size_t numPatients = 3;
    const std::string patients[numPatients] = {
        "John Smith", "Jane Smith", "Joe Smith"
    };
    std::vector<pthread_t> populateThreads;

    for(int i = 0; i < numPatients; i++) {
        // allocate context
        buf_populate_ctx_t *ctx = static_cast<buf_populate_ctx_t *>(malloc(sizeof(buf_populate_ctx_t)));
        memset(ctx, 0, sizeof(buf_populate_ctx_t));

        ctx->buf = &request_buffer;
        ctx->name = patients[i];
        ctx->numRequests = n;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, PopulateThreadEntry, ctx);

        if(err != 0) {
            perror("pthread_create: ");
            abort();
        }

        // add it to the list
        populateThreads.push_back(threadHandle);
    }

    // wait on the request buffers to be filled
    for(int i = 0; i < populateThreads.size(); i++) {
        // get the handle
        pthread_t handle = populateThreads[i];

        err = pthread_join(handle, nullptr);

        if(err != 0) {
            perror("pthread_join: ");
            abort();
        }
    }

    cout << "Done populating request buffer" << endl;

    // add quit requests for each worker thread
    cout << "Pushing quit requests... ";
    for(int i = 0; i < w; ++i) {
        request_buffer.push("quit");
    }
    cout << "done." << endl;


    // set up the channel(s)
/*    cout << "Establishing control channel... " << flush;
    RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
    cout << "done." << endl<< flush;

    chan->cwrite("newchannel");
	string s = chan->cread ();
    RequestChannel *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);

    while(true) {
        string request = request_buffer.pop();
		workerChannel->cwrite(request);

		if(request == "quit") {
		   	delete workerChannel;
            break;
        }else{
			string response = workerChannel->cread();
			hist.update (request, response);
		}
    }
    chan->cwrite ("quit");
    delete chan;*/
    cout << "All Done!!!" << endl;

	hist.print();
}
