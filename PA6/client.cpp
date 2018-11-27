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
#include <signal.h>

#include "RequestChannel.h"
#include "FIFORequestChannel.h"
#include "MQRequestChannel.h"
#include "SHMRequestChannel.h"

#include "BoundedBuffer.h"
#include "Histogram.h"

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
 * Helper method to create a client-side channel of the right type.
 */
RequestChannel *CreateChannelClient(std::string name, channel_type_t type) {
  RequestChannel *channel = nullptr;

  // handle each type
  switch(type) {
    case kChannelTypeFIFO:
      channel = new FIFORequestChannel(name, RequestChannel::CLIENT_SIDE);
      break;

    /*case kChannelTypeMQ:
      break;

    case kChannelTypeSHM:
      break;*/

    default:
      throw std::runtime_error("unsupported channel type");
  }

  return channel;
}


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
	   	// delete ctx->channel;
      break;
    } else {
      std::string response = ctx->channel->cread();

      // figure out which index buffer the response goes into
      for(int i = 0; i < outBufferMapLen; i++) {
        // does the name match?
        if(request == outBufferMap[i]) {
          // if so, copy the response into that buffer
          ctx->statBuffers[i]->push(response);
          break;
        }
      }
		}

    // print info
    // std::cout << "Have " << ctx->requests->size() << " requests " << std::endl;
  }

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

    // type of IPC mechanism to use (FIFO by default)
    channel_type_t channelType = kChannelTypeFIFO;

    // parse arguments
    int opt = 0;
    while((opt = getopt(argc, argv, "n:w:b:i:")) != -1) {
      switch(opt) {
        // number of requests to make
        case 'n':
          n = atoi(optarg);
          break;
        // number of worker threads
        case 'w':
          w = atoi(optarg);
          break;
        // buffer size
        case 'b':
          b = atoi(optarg);
          break;
        // IPC mechanism
        case 'i': {
          switch(optarg[0]) {
            case 'f':
              channelType = kChannelTypeFIFO;
              break;
            case 'q':
              channelType = kChannelTypeMQ;
              break;
            case 's':
              channelType = kChannelTypeSHM;
              break;

            default:
              std::cout << "unknown channel type " << optarg[0] << std::endl;
              return -1;
          }

          break;
        }
      }
    }

    // if b is still the default, set default n*3
    if(b == -1) {
      b = n * 3;
    }

    // print configuration
    std::cout << "IPC mechanism: ";

    switch(channelType) {
      case kChannelTypeFIFO:
        std::cout << "FIFO" << std::endl;
        break;
      case kChannelTypeMQ:
        std::cout << "Message Queue" << std::endl;
        break;
      case kChannelTypeSHM:
        std::cout << "System V Shared Memory" << std::endl;
        break;
    }

    std::cout << "n == " << n << std::endl;
    std::cout << "w == " << w << std::endl;
    std::cout << "b == " << b << std::endl;

    std::cout << "CLIENT STARTED:" << std::endl;

    // start time
    struct timeval t1, t2;
    double elapsedTime;
    gettimeofday(&t1, NULL);

    // spawn server
    pid_t server = fork();

    if(server == 0) {
      // format IPC type argument
      char ipcType[4];
      memset(&ipcType, 0, sizeof(ipcType));

      switch(channelType) {
        case kChannelTypeFIFO:
          ipcType[0] = 'f';
          break;
        case kChannelTypeMQ:
          ipcType[0] = 'q';
          break;
        case kChannelTypeSHM:
          ipcType[0] = 's';
          break;
      }

      execl("dataserver", (char *) "dataserver", (char *) &ipcType, (char *) nullptr);
    } else {
        // handle errors in fork
        if(server == -1) {
            perror("fork");
            abort();
        }
    }

    // set up the channel(s) to server
    std::cout << "Establishing control channel... " << std::flush;

    RequestChannel *chan = RequestChannel::createClientChannel("control", channelType);

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

    // create as many worker threads as needed
    pthread_t workerThreads[w];
    worker_ctx_t *workerThreadsCtx[w];

    memset(workerThreads, 0, sizeof(workerThreads));
    memset(workerThreadsCtx, 0, sizeof(workerThreadsCtx));

    for(int i = 0; i < w; i++) {
        // create a new request channel for the worker
        chan->cwrite("newchannel");
    		std::string s = chan->cread();

        RequestChannel *workerChannel = RequestChannel::createClientChannel(s, channelType);

        // exit if the worker channel is null
  			if(workerChannel == nullptr) {
  				break;
  			}

        // allocate context
        worker_ctx_t *ctx = static_cast<worker_ctx_t *>(malloc(sizeof(worker_ctx_t)));
        memset(ctx, 0, sizeof(worker_ctx_t));

        // ctx->outBuffers = &outBuffers;
        ctx->statBuffers = reinterpret_cast<BoundedBuffer **>(&outBuffers);
        ctx->requests = &requestBuffer;
        ctx->channel = workerChannel;

        workerThreadsCtx[i] = ctx;

        // create the thread
        pthread_t threadHandle;

        err = pthread_create(&threadHandle, nullptr, WorkerThreadEntry, ctx);

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

    // push a quit command for each worker thread
    for(int i = 0; i < w; i++) {
      requestBuffer.push("quit");
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

        // delete that thread's context and channel
        worker_ctx_t *ctx = workerThreadsCtx[i];

        delete ctx->channel;

        free(ctx);
    }

    // wait for stats threads to be done
    hist.waitForCompletion();

    // clear the alarm
    alarm(0);
    signal(SIGALRM, SIG_DFL);

    // quit server
    chan->cwrite("quit");

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

    // delete control channel
    delete chan;
}
