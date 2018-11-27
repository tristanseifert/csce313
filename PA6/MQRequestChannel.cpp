#include "MQRequestChannel.h"

#include <string>
#include <stdexcept>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/**
 * Initializes the request channel.
 */
MQRequestChannel::MQRequestChannel(const std::string _name, const Side _side) : RequestChannel(_name, _side) {

}

/**
 * Cleans up the shared memory request channel.
 */
MQRequestChannel::~MQRequestChannel() {

}



/**
 * Reads a message from the channel.
 */
std::string MQRequestChannel::cread(void) {
  throw std::runtime_error("unimplemented");
}

/**
 * Writes a message to the channel.
 */
int MQRequestChannel::cwrite(std::string msg) {
  throw std::runtime_error("unimplemented");
}
