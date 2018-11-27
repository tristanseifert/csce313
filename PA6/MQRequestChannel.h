#ifndef MQREQUESTCHANNEL_H
#define MQREQUESTCHANNEL_H

#include <string>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "RequestChannel.h"

class MQRequestChannel : public RequestChannel {
  friend struct mq_message;

  public:
    MQRequestChannel(const std::string name, const Side side);
    virtual ~MQRequestChannel();

  public:
    virtual std::string cread();
    virtual int cwrite(std::string msg);

  private:
    void setQueueParams(int queueId);

  private:
    /// queue size
    static const size_t kQueueSize = (1024 * 2);
    /// max message size
    static const size_t kMaxMsgSize = 256;

  private:
    /// key for shared memory region (client write)
    key_t keyClient;
    /// identifier for the message queue (client write)
    int queueIdClient;

    /// key for shared memory region (server write)
    key_t keyServer;
    /// identifier for the message queue (server write)
    int queueIdServer;
};


/**
 * Struct encompassing a single message: this is just the identifier, string
 * length, and the actual NULL-terminated string length.
 */
typedef struct mq_message {
  /// type, must be > 0
  long mtype;

  /// payload data (C string)
  char payload[MQRequestChannel::kMaxMsgSize];
} mq_message_t;

#endif
