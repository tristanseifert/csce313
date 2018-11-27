#include "MQRequestChannel.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <cstring>

#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/// log message send/receives
#define LOG_MSG                     0

/**
 * Initializes the request channel.
 */
MQRequestChannel::MQRequestChannel(const std::string _name, const Side _side) : RequestChannel(_name, _side) {
  int err;

  // create client end of the kerjigger
  std::string clientFilename = this->getFileName(CLIENT_SIDE);
  this->createFile(clientFilename);

  this->keyClient = this->keyForFile(clientFilename);
  this->queueIdClient = msgget(this->keyClient, IPC_CREAT | 0666);

  if(this->queueIdClient == -1) {
    this->handleError("msgget (client end) failed");
  }

  // create server end of the kerjigger
  std::string serverFilename = this->getFileName(SERVER_SIDE);
  this->createFile(serverFilename);

  this->keyServer = this->keyForFile(serverFilename);
  this->queueIdServer = msgget(this->keyServer, IPC_CREAT | 0666);

  if(this->queueIdServer == -1) {
    this->handleError("msgget (server end) failed");
  }

  // set params
  this->setQueueParams(this->queueIdClient);
  this->setQueueParams(this->queueIdServer);

#if LOG_MSG
  std::cout << getpid() << " - client side " << this->queueIdClient << ", server side " << this->queueIdServer << std::endl;
#endif
}

/**
 * Sets queue parameters.
 */
void MQRequestChannel::setQueueParams(int queueId) {
  int err;

  // get initial paramters
  struct msqid_ds info;

  err = msgctl(queueId, IPC_STAT, &info);

  if(err != 0) {
    this->handleError("msgctl IPC_STAT failed");
  }

  // std::cout << "queue size: " << info.msg_qbytes << std::endl;

  // update the max size
  info.msg_qbytes = kQueueSize;

  err = msgctl(queueId, IPC_SET, &info);

  if(err != 0) {
    this->handleError("msgctl IPC_SET failed");
  }
}

/**
 * Cleans up the shared memory request channel.
 */
MQRequestChannel::~MQRequestChannel() {
  int err;

  // close the client queue
  err = msgctl(this->queueIdClient, IPC_RMID, nullptr);

  if(err != 0) {
    this->handleError("msgctl IPC_RMID (client) failed");
  }

  // close the server queue
  err = msgctl(this->queueIdServer, IPC_RMID, nullptr);

  if(err != 0) {
    this->handleError("msgctl IPC_RMID (server) failed");
  }

  // std::cout << "deleted queue " << this->queueId << "(" << this->name << ")" << std::endl;
}



/**
 * Reads a message from the channel.
 */
std::string MQRequestChannel::cread(void) {
  int err;

  // allocate a read buffer
  char *buf = new char[kMaxMsgSize];
  memset(buf, 0, kMaxMsgSize);

  mq_message_t *msg = reinterpret_cast<mq_message_t *>(buf);

  do {
    // read a message
    int queue = (this->side == SERVER_SIDE) ? this->queueIdServer : this->queueIdClient;
    err = msgrcv(queue, buf, kMaxMsgSize, 0, 0);

    if(err == -1) {
      // was the call interrupted?
      if(errno != EINTR) {
        err = 0;
        break;
      } else {
        this->handleError("msgrcv failed");
      }
    }
  } while(err == -1);

  // convert payload
  std::string payload = std::string(msg->payload);

#if LOG_MSG
  std::cout << getpid() << " - received message: " << payload << std::endl << std::flush;
#endif

  // clean up
  delete[] buf;

  // return decoded message
  return payload;
}

/**
 * Writes a message to the channel.
 *
 * If there is insufficient space in the queue, this call blocks.
 */
int MQRequestChannel::cwrite(std::string _msg) {
  int err;

  // get payload and validate its size
  const char *payload = _msg.c_str();
  size_t payloadLen = strlen(payload) + 1;

  if(payloadLen > kMaxMsgSize) {
    throw std::invalid_argument("message is too big");
  }

  // allocate message and clear it
  size_t bufLen = sizeof(mq_message_t) + payloadLen;

  char *buf = new char[bufLen];
  memset(buf, 0, bufLen);

  // fill in message and copy string
  mq_message_t *msg = reinterpret_cast<mq_message_t *>(buf);

  msg->mtype = 1;

  strncpy(msg->payload, payload, payloadLen);

#if LOG_MSG
  std::cout << getpid() << " - sent message: " << _msg << std::endl << std::flush;
  // std::cout << "sending buffer " << std::hex << msg << std::dec << " size " << bufLen << std::endl;
#endif

  // send message
  int queue = (this->side == CLIENT_SIDE) ? this->queueIdServer : this->queueIdClient;
  err = msgsnd(queue, msg, payloadLen, 0);

  if(err == -1) {
    // was it invalid arguments?
    if(errno == EINVAL) {
      // check that the queue exists
      struct msqid_ds info;
      err = msgctl(queue, IPC_STAT, &info);

      // some other queue related error happened
      if(err == -1 && (errno != EINVAL && errno != EIDRM)) {
        this->handleError("msgsnd failed (after msgctl)");
      }
    }
    // otherwise, handle it like any other error
    else {
      this->handleError("msgsnd failed");
    }
  }

  // clean up
  delete[] buf;

  // return number of bytes (of payload data) written
  // return bufLen;
  return payloadLen;
}
