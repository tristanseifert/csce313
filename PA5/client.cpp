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
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>

#include "reqchannel.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
using namespace std;

/// produce logging around reading from request channels and their lifetime
#define SELECT_LOGGING              0

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

  /// request channel sto server
  std::vector<RequestChannel *> channels;

  /// output buffer list
  BoundedBuffer **statBuffers;
} worker_ctx_t;

/**
 * Data type passed to the stats threads.
 */
typedef struct {
  /// Histogram to modify
  Histogram *hist;
  /// Bounded buffer from which data is read
  BoundedBuffer *statBuffer;

  /// name of the patient
  std::string name;
  /// how many pieces of data we need before histogram is complete
  int numResponses;
} stats_ctx_t;

/**
 * Structure passed to the alarm handler.
 */
typedef struct {
  /// Histogram to print data from
  Histogram *hist;
} alarm_ctx_t;
alarm_ctx_t *g_alarmCtx = nullptr;



/**
 * Alarm handler: this is called every two seconds and will signal the status
 * display thread.
 */
void AlarmHandler(int signum) {
  // clear screen and move cursor to top
  std::cout << "\033[2J\033[H" << std::flush;

  // print the histogram
  g_alarmCtx->hist->print();

  // set alarm again
  alarm(2);
}


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

  // done
  return nullptr;
}

/**
 * Entry point for the worker thread.
 */
void *WorkerThreadEntry(void *_ctx) {
  int err;

  // which buffer index is for which requests
  // XXX: this sucks ass and will break if we add more patients
  const std::size_t outBufferMapLen = 3;
  const std::string outBufferMap[outBufferMapLen] = {
    "data John Smith", "data Jane Smith", "data Joe Smith"
  };

  worker_ctx_t *ctx = static_cast<worker_ctx_t *>(_ctx);

  // this maps the last request written to a channel
  std::map<RequestChannel *, std::string> requestMap;

  // file descriptors to close later
  std::vector<RequestChannel *> channels;

  // write a request to every channel
  for(auto it = ctx->channels.begin(); it != ctx->channels.end(); it++) {
    // do we have a request to dequeue?
    if(!ctx->requests->isEmpty()) {
      // yep, send it
      std::string request = ctx->requests->pop();
      (*it)->cwrite(request);

      // we shouldn't have a quit just yet, but sorta handle it anwyays
      if(request == "quit") {
        delete (*it);
        // TODO: remove from vector
      }

      requestMap[(*it)] = request;
    } else {
      std::cout << "ran out of requests when priming work channels" << std::endl;
    }
  }

  // set up for the select call
  fd_set fds;
  FD_ZERO(&fds);

  // handle the requests
  while(true) {
    // make sure we monitor all open channels
    FD_ZERO(&fds);

    int highestFd = 0;

#if SELECT_LOGGING
    std::cout << "FDs: ";
#endif

    for(auto it = ctx->channels.begin(); it != ctx->channels.end(); it++) {
      int fd = (*it)->read_fd();

#if SELECT_LOGGING
      std::cout << fd << " ";
#endif

      // store it if it's the highest fd (for select() call)
      if(fd > highestFd) {
        highestFd = fd;
      }

      // set it in the map
      FD_SET(fd, &fds);
    }

#if SELECT_LOGGING
    std::cout << std::endl;
#endif

    // if no FDs need to be read, we're done
    if(highestFd == 0) {
      break;
    }

    // now wait on all of those file dscriptors
    err = select((highestFd + 1), &fds, nullptr, nullptr, nullptr);

    if(err == -1) {
      std::cout << "select() failed: " << errno << std::endl;
      abort();
    }

    // std::cout << err << " channels ready" << std::endl;

    // read from the channels
    for(auto it = ctx->channels.begin(); it != ctx->channels.end();) {
      int fd = (*it)->read_fd();

      // did this channel get set?
      if(FD_ISSET(fd, &fds)) {
        // read from it
        std::string request = requestMap[(*it)];
        std::string response = (*it)->cread();

        // figure out which index buffer the response goes into
        for(int i = 0; i < outBufferMapLen; i++) {
          // does the name match?
          if(request == outBufferMap[i]) {
            // if so, copy the response into that buffer
            ctx->statBuffers[i]->push(response);
            break;
          }
        }

        // also, write a new request to it
        request = ctx->requests->pop();
        (*it)->cwrite(request);

        requestMap[(*it)] = request;

        // was this a quit request?
        if(request == "quit") {
#if SELECT_LOGGING
          std::cout << "Closing channel " << fd << std::endl;
#endif

          // remove the channel from the vector and close it later
          it = ctx->channels.erase(it);
          channels.push_back((*it));
        }
        // if not, simply go to the next channel instead
        else {
          it++;
        }
      }
      // if not, check the next one
      else {
        it++;
        continue;
      }
    }

    // print info
    // std::cout << "Have " << ctx->requests->size() << " requests " << std::endl;
  }

  // close all file descriptors
  /*for(auto it = channels.begin(); it != channels.end(); it++) {
    delete (*it);
  }*/

  // done
  return nullptr;
}

/**
 * Adds data for a particular patient to the histogram.
 */
void *StatsThreadEntry(void *_ctx) {
  stats_ctx_t *ctx = static_cast<stats_ctx_t *>(_ctx);
  std::string histogramKey = "data " + ctx->name;

  // count the number of responses we've gotten
  int responses = 0;

  // continuously consume data from the buffer
  while(true) {
    // get data out of the buffer and update the histogram
    std::string data = ctx->statBuffer->pop();

    ctx->hist->update(histogramKey, data);

    // is this enough data?
    responses++;

    // std::cout << "Got " << responses << " for " << histogramKey
      // << " (need " << ctx->numResponses << ")" << std::endl;

    if(responses == ctx->numResponses) {
      // std::cout << "Finished histogram for " << histogramKey << std::endl;
      break;
    }
  }

  // mark histogram as complete
  ctx->hist->markBinAsComplete(histogramKey);

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
    int numWorkerThreads = 1;

    // parse arguments
    int opt = 0;
    while((opt = getopt(argc, argv, "n:w:b:t:")) != -1) {
      switch (opt) {
        case 'n': // number of requests per person
          n = atoi(optarg);
          break;
        case 'w': // number of channels
          w = atoi(optarg);
          break;
        case 'b': // bounded buffer size
          b = atoi(optarg);
          break;
        case 't': // number of threads
          numWorkerThreads = atoi(optarg);
          break;
      }
    }

    // w should be less than 3x n
    if(w >= (n * 3)) {
      std::cout << "w should be less than 3n" << std::endl;
      return -1;
    }
    // also make sure that w is no more than 1/2 FD_SETSIZE
    if(w >= ((FD_SETSIZE / 2) - 1)) {
      std::cout << "w should be less than half FD_SETSIZE (" << FD_SETSIZE << ")"
                << std::endl;
      return -1;
    }
    // we shouldn't have more worker threads than channels
    if(numWorkerThreads > w) {
      std::cout << "Can't have more worker threads than channels" << std::endl;
      return -1;
    }

    // if b is still the default, set default n*3
    if(b == -1) {
      b = n * 3;
    }

    std::cout << "n == " << n << std::endl;
    std::cout << "w == " << w << std::endl;
    std::cout << "b == " << b << std::endl;
    std::cout << "worker threads == " << numWorkerThreads << std::endl;

    std::cout << "CLIENT STARTED:" << endl;

    // start time
    struct timeval t1, t2;
    double elapsedTime;
    gettimeofday(&t1, NULL);

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
    std::cout << "Establishing control channel... " << std::flush;
    RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
    std::cout << "done." << std::endl << std::flush;

    // this is the buffer and histogram used by everything
    BoundedBuffer requestBuffer(b);
    Histogram hist;



    // install the alarm handler and set it to fire every 2 sec
    g_alarmCtx = static_cast<alarm_ctx_t *>(malloc(sizeof(alarm_ctx_t)));
    memset(g_alarmCtx, 0, sizeof(alarm_ctx_t));

    g_alarmCtx->hist = &hist;

    signal(SIGALRM, AlarmHandler);
    alarm(2);

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

        // name the thread, if supported
#ifdef pthread_setname_np
        const std::size_t nameLen = 48;
        char name[nameLen];

        memset(name, 0, nameLen);
        snprintf(name, nameLen, "Request %d (%s)", i, patients[i].c_str());

        err = pthread_setname_np(threadHandle, name);

        if(err != 0) {
          std::cout << "pthread_setname_np: " << err << std::endl;
        }
#endif

        // add it to the list
        requestThreads[i] = threadHandle;
    }

    std::cout << "Done spawning request threads" << std::endl;

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
      ctx->statBuffer = outBuffers[i];
      ctx->numResponses = n;

      statsThreadsCtx[i] = ctx;

      // create the thread
      pthread_t threadHandle;

      err = pthread_create(&threadHandle, nullptr, StatsThreadEntry, ctx);

      if(err != 0) {
          perror("pthread_create - stats");
          abort();
      }

      // name the thread, if supported
#ifdef pthread_setname_np
      const std::size_t nameLen = 48;
      char name[nameLen];

      memset(name, 0, nameLen);
      snprintf(name, nameLen, "Stats %d (%s)", i, patients[i].c_str());

      err = pthread_setname_np(threadHandle, name);

      if(err != 0) {
        std::cout << "pthread_setname_np: " << err << std::endl;
      }
#endif

      // add it to the list
      statsThreads[i] = threadHandle;
    }

    std::cout << "Done spawning stats threads" << std::endl;

    // create as many channels to the server threads as needed
    pthread_t workerThreads[numWorkerThreads];
    worker_ctx_t *workerThreadsCtx[numWorkerThreads];

    std::vector<RequestChannel *> channels;

    for(int i = 0; i < w; i++) {
      // create a new request channel for the worker
      chan->cwrite("newchannel");
  		string s = chan->cread();
      RequestChannel *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);

      // exit if the worker channel is null
			if(workerChannel == nullptr) {
				break;
			}

      // add it
      channels.push_back(workerChannel);
    }

    // create the worker threads' contexts
    for(int i = 0; i < numWorkerThreads; i++) {
      worker_ctx_t *ctx = new worker_ctx_t();

      ctx->statBuffers = reinterpret_cast<BoundedBuffer **>(&outBuffers);
      ctx->requests = &requestBuffer;

      workerThreadsCtx[i] = ctx;
    }


    // divide channels between the threads
    int i = 0;
    for(auto it = channels.begin(); it != channels.end(); i++, it++) {
      int thread = (i % numWorkerThreads);

      workerThreadsCtx[thread]->channels.push_back((*it));
    }

    // and actually create the worker thread now
    for(int i = 0; i < numWorkerThreads; i++) {
      // create the thread
      pthread_t threadHandle;

      err = pthread_create(&threadHandle, nullptr, WorkerThreadEntry, workerThreadsCtx[i]);

      if(err != 0) {
          perror("pthread_create - worker");
          abort();
      }

        // name the thread, if supported
#ifdef pthread_setname_np
      const std::size_t nameLen = 48;
      char name[nameLen];

      memset(name, 0, nameLen);
      snprintf(name, nameLen, "Worker %d", i);

      err = pthread_setname_np(threadHandle, name);

      if(err != 0) {
        std::cout << "pthread_setname_np: " << err << std::endl;
      }
  #endif

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

    // push a quit command for each worker channel
    for(int i = 0; i < w; i++) {
      requestBuffer.push("quit");
    }

    // wait for workers to join and delete its context
    for(int i = 0; i < numWorkerThreads; i++) {
      err = pthread_join(workerThreads[i], nullptr);

      if(err != 0) {
          perror("pthread_join - worker threads");
          abort();
      }

      // also delete its context, lol
      delete workerThreadsCtx[i];
    }

    // delete the worker channels
    for(auto it = channels.begin(); it != channels.end(); it++) {
      delete (*it);
    }

    // wait for stats threads to be done
    hist.waitForCompletion();

    // clear the alarm
    alarm(0);
    signal(SIGALRM, SIG_DFL);

    // quit server
    chan->cwrite("quit");
    delete chan;

    // calculate difference
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.f;
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.f;

    std::cout << "Took " << (elapsedTime / 1000.f) << " seconds " << std::endl;

    // print histogram machine
    std::cout << "All Done!!!" << std::endl;

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

        delete outBuffers[i];
    }
}
