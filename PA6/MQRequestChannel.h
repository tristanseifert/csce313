#ifndef MQREQUESTCHANNEL_H
#define MQREQUESTCHANNEL_H

#include <string>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "RequestChannel.h"

/**
 * Struct encompassing a single message: this is just the identifier, string
 * length, and the actual NULL-terminated string length.
 */
typedef struct {
  /// type, must be > 0
  long type;

  /// number of bytes of data following, including NULL byte
  size_t payloadLen;
  /// payload data (C string)
  char payload[];
} mq_message_t;

class MQRequestChannel : public RequestChannel {
  public:
    MQRequestChannel(const std::string name, const Side side);
    virtual ~MQRequestChannel();

  public:
    virtual std::string cread();
    virtual int cwrite(std::string msg);
};

#endif
