#include "RequestChannel.h"

#include "FIFORequestChannel.h"
#include "MQRequestChannel.h"
#include "SHMRequestChannel.h"

#include <string>
#include <sstream>
#include <stdexcept>

#include <typeinfo>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/**
 * Constructs the base request channel class: assigns variables and such.
 */
RequestChannel::RequestChannel(const std::string name, const Side side) {
  if(name.size() == 0) {
    throw std::invalid_argument("name may not be an empty string");
  }

  this->name = name;
  this->side = side;
}

/**
 * Deletes any files that we allocated.
 */
RequestChannel::~RequestChannel() {
  for(auto it = this->files.begin(); it != this->files.end(); it++) {
    // delete the file
    this->deleteFile(*it);
  }
}

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

    case kChannelTypeMQ:
      channel = new MQRequestChannel(name, side);
      break;

    case kChannelTypeSHM:
      channel = new SHMRequestChannel(name, side);
      break;

    default:
      throw std::runtime_error("unsupported channel type");
  }

  return channel;
}


/**
 * Returns the filename for a temporary file to identify a channel.
 */
std::string RequestChannel::getFileName(Mode mode) {
	std::string pname;

  // append type
  if(typeid(*this) == typeid(FIFORequestChannel)) {
    pname += "fifo";
  } else if(typeid(*this) == typeid(MQRequestChannel)) {
    pname += "mq";
  } else if(typeid(*this) == typeid(SHMRequestChannel)) {
    pname += "shm";
  }

  // append name
  pname += "_" + this->name;

  // add the number to indicate which end of the channel it is for FIFOs
  if(typeid(*this) == typeid(FIFORequestChannel)) {
  	if(this->side == CLIENT_SIDE) {
  		if(mode == READ_MODE) {
  			pname += "1";
  		} else {
  			pname += "2";
      }
  	}
  	else {
  	/* SERVER_SIDE */
  		if(mode == READ_MODE) {
  			pname += "2";
  		} else {
  			pname += "1";
      }
  	}
  }

	return pname;
}

/**
 * Creates a temporary file.
 */
void RequestChannel::createFile(std::string name) {
  // attempt create it
  int fd = open(name.c_str(), O_RDWR | O_CREAT, 0660);

  if(fd < 0 && errno != EEXIST) {
    this->handleError("open failed");
  }

  // add its name to the internal structure
  this->files.push_back(name);

  // close it again
  close(fd);
}

/**
 * Deletes the given file.
 */
void RequestChannel::deleteFile(std::string name) {
  // attempt to delete file
  int err = unlink(name.c_str());

  // handle errors
  if(err == -1 && errno != ENOENT) {
    this->handleError("unlink failed");
  }
}


/**
 * Handles a generic system error. This creates an exception with the supplied
 * description, and the error number.
 */
void RequestChannel::handleError(std::string description) {
  std::stringstream msg;

  msg << description;
  msg << ": ";
  msg << strerror(errno);

  throw std::runtime_error(msg.str());
}
