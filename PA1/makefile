# makefile

all: memtest

Ackerman.o: Ackerman.cpp
	g++ -c -g -std=c++11 Ackerman.cpp

BuddyAllocator.o : BuddyAllocator.cpp
	g++ -c -g -std=c++11 BuddyAllocator.cpp

Main.o : Main.cpp
	g++ -c -g -std=c++11 Main.cpp

memtest: Main.o Ackerman.o BuddyAllocator.o
	g++ -o memtest Main.o Ackerman.o BuddyAllocator.o

clean:
	rm *.o
	rm memtest
