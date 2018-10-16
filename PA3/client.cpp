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
	/// buffer into which to insert requests
	SafeBuffer *buf;

	/// person for whom to get info
	std::string name;
	/// how many requests to make
	size_t numRequests;
} buf_populate_ctx_t;

/**
 * Data type passed to the threads that process requests.
 */
typedef struct {
    /// Histogram that is updated by data responses
    Histogram *hist;
	/// buffer from which to get requests
	SafeBuffer *requests;

    /// request channel to server
    RequestChannel *channel;
} worker_ctx_t;



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

/**
 * Entry point for the worker thread.
 */
void *WorkerThreadEntry(void *_ctx) {
    worker_ctx_t *ctx = static_cast<worker_ctx_t *>(_ctx);

    // handle the requests
    while(true) {
        std::string request = ctx->requests->pop();
		ctx->channel->cwrite(request);

		if(request == "quit") {
            // TODO: problems if doing this on another thread?
		   	delete ctx->channel;
            break;
        } else {
            // handle the histogram
			string response = ctx->channel->cread();
			ctx->hist->update(request, response);
		}
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

    // parse arguments
    int opt = 0;
    while ((opt = getopt(argc, argv, "n:w:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
			}
    }

    cout << "n == " << n << endl;
    cout << "w == " << w << endl;

    cout << "CLIENT STARTED:" << endl;

    // spawn server
    pid_t server = fork();

    if(server == 0) {
        execl("dataserver", (char *) nullptr);
    } else {
        // handle errors in fork
        if(server == -1) {
            perror("fork: ");
            abort();
        }
    }

    // set up the channel(s) to server
    cout << "Establishing control channel... " << flush;
    RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
    cout << "done." << endl << flush;

    // this is the buffer and histogram used by everything
	SafeBuffer request_buffer;
	Histogram hist;



    // create the threads to push requests
    const size_t numPatients = 3;
    const std::string patients[numPatients] = {
        "John Smith", "Jane Smith", "Joe Smith"
    };

    pthread_t populateThreads[numPatients];
    buf_populate_ctx_t *populateThreadsCtx[numPatients];

    memset(populateThreads, 0, sizeof(populateThreads));

    for(int i = 0; i < numPatients; i++) {
        // allocate context
        buf_populate_ctx_t *ctx = static_cast<buf_populate_ctx_t *>(malloc(sizeof(buf_populate_ctx_t)));
        memset(ctx, 0, sizeof(buf_populate_ctx_t));

        ctx->buf = &request_buffer;
        ctx->name = patients[i];
        ctx->numRequests = n;

        populateThreadsCtx[i] = ctx;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, PopulateThreadEntry, ctx);

        if(err != 0) {
            perror("pthread_create: ");
            abort();
        }

        // add it to the list
        populateThreads[i] = threadHandle;
    }

    // wait on the request buffers to be filled
    for(int i = 0; i < numPatients; i++) {
        // get the handle and wait for thread to join
        pthread_t handle = populateThreads[i];

        err = pthread_join(handle, nullptr);

        if(err != 0) {
            perror("pthread_join: ");
            abort();
        }

        // delete that thread's context
        buf_populate_ctx_t *ctx = populateThreadsCtx[i];

        free(ctx);
    }

    cout << "Done populating request buffer" << endl;

    // add quit requests for each worker thread
    cout << "Pushing quit requests... ";
    for(int i = 0; i < w; ++i) {
        request_buffer.push("quit");
    }
    cout << "done." << endl;

    // create as many worker threads as needed
    pthread_t workerThreads[w];
    worker_ctx_t *workerThreadsCtx[w];

    memset(workerThreads, 0, sizeof(workerThreads));

    for(int i = 0; i < w; i++) {
        // create a new request channel for ther worker
        chan->cwrite("newchannel");
    	string s = chan->cread();
        RequestChannel *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);

        // allocate context
        worker_ctx_t *ctx = static_cast<worker_ctx_t *>(malloc(sizeof(worker_ctx_t)));
        memset(ctx, 0, sizeof(worker_ctx_t));

        ctx->hist = &hist;
        ctx->requests = &request_buffer;
        ctx->channel = workerChannel;

        /// request channel to server
        RequestChannel *channel;

        workerThreadsCtx[i] = ctx;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, WorkerThreadEntry, ctx);

        if(err != 0) {
            perror("pthread_create: ");
            abort();
        }

        // add it to the list
        workerThreads[i] = threadHandle;
    }

    // wait for workers to join
    for(int i = 0; i < w; i++) {
        // get the handle and wait for thread to join
        pthread_t handle = workerThreads[i];

        err = pthread_join(handle, nullptr);

        if(err != 0) {
            perror("pthread_join: ");
            abort();
        }

        // delete that thread's context
        worker_ctx_t *ctx = workerThreadsCtx[i];

        free(ctx);
    }

    // quit server
    chan->cwrite("quit");
    delete chan;

    // print histogram machine
    cout << "All Done!!!" << endl;

	hist.print();
}
