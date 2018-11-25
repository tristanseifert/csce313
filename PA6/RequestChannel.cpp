#include "RequestChannel.h"

#include "FIFORequestChannel.h"
#include "MQRequestChannel.h"
#include "SHMRequestChannel.h"

#include <string>
#include <stdexcept>

/**
 * Creates a request channel of the appropriate type.
 */
RequestChannel *RequestChannel::createChannel(std::string name, channel_type_t type, Side side) {
  RequestChannel *channel = nullptr;

  // handle each type
  switch(type) {
    case kChannelTypeFIFO:
      channel = new FIFORequestChannel(name, side);
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
