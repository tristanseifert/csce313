/**
 * Abstract base class for a request channel.
 */
#ifndef REQUESTCHANNEL_H
#define REQUESTCHANNEL_H

#include <string>
#include <stdexcept>

/**
 * Enum defining the different types of request channels.
 */
typedef enum {
  /// unknown type (shouldn't happen)
  kChannelTypeUnknown,

  /// FIFO channel (same as previous PA's)
  kChannelTypeFIFO,
  /// System V shared memory channel
  kChannelTypeSHM,
  /// message queue channel
  kChannelTypeMQ
} channel_type_t;

class RequestChannel {
  public:
    typedef enum {SERVER_SIDE, CLIENT_SIDE} Side;
    typedef enum {READ_MODE, WRITE_MODE} Mode;

  public:
    RequestChannel(const std::string name, const Side side) {
      this->name = name;
      this->side = side;
    }
    virtual ~RequestChannel() { /* do nothing */ }

  public:
    static RequestChannel *createClientChannel(std::string name, channel_type_t type) {
      return RequestChannel::createChannel(name, type, CLIENT_SIDE);
    }

    static RequestChannel *createServerChannel(std::string name, channel_type_t type) {
      return RequestChannel::createChannel(name, type, SERVER_SIDE);
    }

  private:
    static RequestChannel *createChannel(std::string name, channel_type_t type, Side side);

  public:
    /**
     * Blocking read of data from the channel. Returns a string of characters
     * read from the channel, or throws an exception if the read failed.
     */
    virtual std::string cread() {
      throw std::runtime_error("cannot call cread() on abstract base class");
    }

    /**
     * Write data to the channel. Returns the number of characters that were
     * written.
     */
    virtual int cwrite(std::string msg) {
      throw std::runtime_error("cannot call cwrite() on abstract base class");
    }

  protected:
    std::string name;
    Side side;
};

#endif
