#ifndef MQREQUESTCHANNEL_H
#define MQREQUESTCHANNEL_H

#include <string>

#include "RequestChannel.h"

class MQRequestChannel : public RequestChannel {
  public:
    MQRequestChannel(const std::string name, const Side side);
    virtual ~MQRequestChannel();

  public:
    virtual std::string cread();
    virtual int cwrite(std::string msg);
};

#endif
