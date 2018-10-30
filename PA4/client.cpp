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
#include "BoundedBuffer.h"
#include "Histogram.h"
using namespace std;

/**
 * Data type passed to the threads to populate the request buffer.
 */
typedef struct {
	/// buffer into which to insert requests
	BoundedBuffer *buf;

	/// person for whom to get info
	std::string name;
	/// how many requests to make
	size_t numRequests;
} buf_populate_ctx_t;

/**
 * Data type passed to the threads that process requests.
 */
typedef struct {
	/// buffer from which to get requests
	BoundedBuffer *requests;

  /// request channel to server
  RequestChannel *channel;

  /// output buffer list
  BoundedBuffer **outBuffers;
} worker_ctx_t;

/**
 * Data type passed to the stats threads.
 */
typedef struct {
  /// Histogram to modify
  Histogram *hist;
  /// Bounded buffer from which data is read
  BoundedBuffer *dataBuffer;

  /// name of the patient
  std::string name;
} stats_ctx_t;



/**
 * Entry point for the request thread.
 */
void *RequestThreadEntry(void *_ctx) {
  buf_populate_ctx_t *ctx = static_cast<buf_populate_ctx_t *>(_ctx);

  // create the command to push
  std::string command = "data " + ctx->name;

  // push it N times
  for(size_t i = 0; i < ctx->numRequests; i++) {
      ctx->buf->push(command);
  }

  // also, push a quit command
  ctx->buf->push("quit");

  // done
  return nullptr;
}

/**
 * Entry point for the worker thread.
 */
void *WorkerThreadEntry(void *_ctx) {
  // which buffer index is for which requests
  // XXX: this sucks ass and will break if we add more patients
  const std::size_t outBufferMapLen = 3;
  const std::string outBufferMap[outBufferMapLen] = {
    "data John Smith", "data Jane Smith", "data Joe Smith"
  };

  worker_ctx_t *ctx = static_cast<worker_ctx_t *>(_ctx);

  // handle the requests
  while(true) {
    std::string request = ctx->requests->pop();
		ctx->channel->cwrite(request);

		if(request == "quit") {
	   	delete ctx->channel;
      break;
    } else {
      string response = ctx->channel->cread();

      // figure out which index buffer the response goes into
      for(int i = 0; i < outBufferMapLen; i++) {
        // does the name match?
        if(request == outBufferMap[i]) {
          // if so, copy the response into that buffer
          ctx->outBuffers[i]->push(response);
          break;
        }
      }
		}
  }

  // done
  return nullptr;
}

/**
 * Adds data for a particular patient to the histogram.
 */
void *StatsThreadEntry(void *_ctx) {
  stats_ctx_t *ctx = static_cast<stats_ctx_t *>(_ctx);

  // continuously consume data from the buffer
  while(true) {
    std::string data = ctx->dataBuffer->pop();

    std::string key = "data " + ctx->name;
    ctx->hist->update(key, data);
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
    int b = -1;

    // parse arguments
    int opt = 0;
    while((opt = getopt(argc, argv, "n:w:b:")) != -1) {
      switch (opt) {
        case 'n':
          n = atoi(optarg);
          break;
        case 'w':
          w = atoi(optarg);
          break;
        case 'b':
          b = atoi (optarg);
          break;
      }
    }

    // if b is still the default, set default n*3
    if(b == -1) {
      b = n * 3;
    }

    cout << "n == " << n << endl;
    cout << "w == " << w << endl;
    cout << "b == " << b << endl;

    cout << "CLIENT STARTED:" << endl;

    // start time
    time_t timeStart = clock();

    // spawn server
    pid_t server = fork();

    if(server == 0) {
        execl("dataserver", (char *) nullptr);
    } else {
        // handle errors in fork
        if(server == -1) {
            perror("fork");
            abort();
        }
    }

    // set up the channel(s) to server
    cout << "Establishing control channel... " << flush;
    RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
    cout << "done." << endl << flush;

    // this is the buffer and histogram used by everything
    BoundedBuffer requestBuffer(b);
    Histogram hist;



    // create the threads to push requests
    const size_t numPatients = 3;
    const std::string patients[numPatients] = {
        "John Smith", "Jane Smith", "Joe Smith"
    };

    pthread_t requestThreads[numPatients];
    buf_populate_ctx_t *requestThreadsCtx[numPatients];

    memset(requestThreads, 0, sizeof(requestThreads));
    memset(requestThreadsCtx, 0, sizeof(requestThreadsCtx));

    for(int i = 0; i < numPatients; i++) {
        // allocate context
        buf_populate_ctx_t *ctx = static_cast<buf_populate_ctx_t *>(malloc(sizeof(buf_populate_ctx_t)));
        memset(ctx, 0, sizeof(buf_populate_ctx_t));

        ctx->buf = &requestBuffer;
        ctx->name = patients[i];
        ctx->numRequests = n;

        requestThreadsCtx[i] = ctx;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, RequestThreadEntry, ctx);

        if(err != 0) {
            perror("pthread_create - request");
            abort();
        }

        // add it to the list
        requestThreads[i] = threadHandle;
    }

    cout << "Done spawning request threads" << endl;

    // create a stats thread for each of the patients
    pthread_t statsThreads[numPatients];
    stats_ctx_t *statsThreadsCtx[numPatients];
    BoundedBuffer *outBuffers[numPatients];

    memset(statsThreads, 0, sizeof(statsThreads));
    memset(statsThreadsCtx, 0, sizeof(statsThreadsCtx));
    memset(outBuffers, 0, sizeof(outBuffers));

    for(int i = 0; i < numPatients; i++) {
      // allocate an output buffer for this queue
      outBuffers[i] = new BoundedBuffer((b / 3));

      // allocate context
      stats_ctx_t *ctx = static_cast<stats_ctx_t *>(malloc(sizeof(stats_ctx_t)));
      memset(ctx, 0, sizeof(stats_ctx_t));

      ctx->hist = &hist;
      ctx->name = patients[i];
      ctx->dataBuffer = outBuffers[i];

      statsThreadsCtx[i] = ctx;

      // create the thread
      pthread_t threadHandle;

      err = pthread_create(&threadHandle, nullptr, StatsThreadEntry, ctx);

      if(err != 0) {
          perror("pthread_create - stats");
          abort();
      }

      // add it to the list
      statsThreads[i] = threadHandle;
    }

    cout << "Done spawning stats threads" << endl;

    // create as many worker threads as needed
    pthread_t workerThreads[w];
    worker_ctx_t *workerThreadsCtx[w];

    memset(workerThreads, 0, sizeof(workerThreads));
    memset(workerThreadsCtx, 0, sizeof(workerThreadsCtx));

    for(int i = 0; i < w; i++) {
        // create a new request channel for the worker
        chan->cwrite("newchannel");
    		string s = chan->cread();
        RequestChannel *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);

        // exit if the worker channel is null
  			if(workerChannel == nullptr) {
  				break;
  			}

        // allocate context
        worker_ctx_t *ctx = static_cast<worker_ctx_t *>(malloc(sizeof(worker_ctx_t)));
        memset(ctx, 0, sizeof(worker_ctx_t));

        // ctx->outBuffers = &outBuffers;
        ctx->outBuffers = reinterpret_cast<BoundedBuffer **>(&outBuffers);
        ctx->requests = &requestBuffer;
        ctx->channel = workerChannel;

        /// request channel to server
        workerThreadsCtx[i] = ctx;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, WorkerThreadEntry, ctx);

        if(err != 0) {
            perror("pthread_create - worker");
            abort();
        }

        // add it to the list
        workerThreads[i] = threadHandle;
    }

    std::cout << "Done spawning worker threads" << std::endl;



    // wait for request threads to join
    for(int i = 0; i < numPatients; i++) {
        // get the handle and wait for thread to join
        pthread_t handle = requestThreads[i];

        err = pthread_join(handle, nullptr);

        if(err != 0) {
            perror("pthread_join - request threads");
            abort();
        }

        // delete that thread's context
        buf_populate_ctx_t *ctx = requestThreadsCtx[i];
        free(ctx);
    }

    // wait for workers to join
    for(int i = 0; i < w; i++) {
        // get the handle and wait for thread to join
        pthread_t handle = workerThreads[i];

        err = pthread_join(handle, nullptr);

        if(err != 0) {
            perror("pthread_join - worker threads");
            abort();
        }

        // delete that thread's context
        worker_ctx_t *ctx = workerThreadsCtx[i];
        free(ctx);
    }

    // quit server
    chan->cwrite("quit");
    delete chan;

    // wait for stats threads to be done
    // TODO: implement

    // calculate difference
    time_t timeEnd = clock();
    double totalTimeSecs = ((double) (timeEnd - timeStart)) / CLOCKS_PER_SEC;

    std::cout << "Took " << totalTimeSecs << " seconds " << std::endl;

    // print histogram machine
    cout << "All Done!!!" << endl;

  	hist.print();

    // delete resources used by the stats threads
    for(int i = 0; i < numPatients; i++) {
        // get the handle and wait for thread to join
        pthread_t handle = statsThreads[i];

        // err = pthread_join(handle, nullptr);
        err = pthread_cancel(handle);

        if(err != 0) {
            perror("pthread_join - stats threads");
            abort();
        }

        // delete that thread's context and buffer
        stats_ctx_t *ctx = statsThreadsCtx[i];
        free(ctx);

        // XXX: this sometimes gives errors, we technically leak memory now
        // delete outBuffers[i];
    }
}
