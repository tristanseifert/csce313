# makefile
all: dataserver client

# C++ compiler flags
CFLAGS = -g -w -Wall -O1 -std=c++17

# linker flags for libraries
LIBFLAGS = -lpthread

# object files for all channels
CHANNEL_OBJS = RequestChannel.o FIFORequestChannel.o MQRequestChannel.o SHMRequestChannel.o KernelSemaphore.o

# client specific stuff
BoundedBuffer.o: BoundedBuffer.h BoundedBuffer.cpp
	g++ $(CFLAGS) -c BoundedBuffer.cpp

Histogram.o: Histogram.h Histogram.cpp
	g++ $(CFLAGS) -c Histogram.cpp

# rules to build request channels
channels: $(CHANNEL_OBJS)

RequestChannel.o: RequestChannel.h RequestChannel.cpp
	g++ $(CFLAGS) -c RequestChannel.cpp

FIFORequestChannel.o: FIFORequestChannel.h FIFORequestChannel.cpp
	g++ $(CFLAGS) -c FIFORequestChannel.cpp
MQRequestChannel.o: MQRequestChannel.h MQRequestChannel.cpp
	g++ $(CFLAGS) -c MQRequestChannel.cpp
SHMRequestChannel.o: SHMRequestChannel.h SHMRequestChannel.cpp
	g++ $(CFLAGS) -c SHMRequestChannel.cpp

KernelSemaphore.o: KernelSemaphore.h KernelSemaphore.cpp
	g++ $(CFLAGS) -c KernelSemaphore.cpp


# build executables
dataserver: dataserver.cpp channels
	g++ $(CFLAGS) -o dataserver dataserver.cpp $(CHANNEL_OBJS) $(LIBFLAGS)

client: client.cpp channels BoundedBuffer.o Histogram.o KernelSemaphore.o
	g++ $(CFLAGS) -o client client.cpp $(CHANNEL_OBJS) BoundedBuffer.o Histogram.o $(LIBFLAGS)

# cleans up build artifacts
clean: cleanfifo
	rm -rf *.o *.dSYM dataserver client
cleanfifo:
# fifo files have an underscore in them
	rm -rf fifo_* mq_* shm_*
