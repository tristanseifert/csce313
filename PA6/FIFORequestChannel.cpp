/*
    File: FIFORequestChannel.C

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2012/07/11

*/
#include "FIFORequestChannel.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

void EXITONERROR(std::string msg);

void EXITONERROR(std::string msg){
	perror(msg.c_str());
	exit(-1);
}
std::string FIFORequestChannel::pipe_name(Mode _mode) {
	std::string pname = "fifo_" + this->name;

	if(this->side == CLIENT_SIDE) {
		if(_mode == READ_MODE)
			pname += "1";
		else
			pname += "2";
	}
	else {
	/* SERVER_SIDE */
		if(_mode == READ_MODE)
			pname += "2";
		else
			pname += "1";
	}
	return pname;
}
void FIFORequestChannel::create_pipe(std::string _pipe_name){
	mkfifo(_pipe_name.c_str(), 0600) < 0; //{
	//	EXITONERROR(_pipe_name);
	//}
}


void FIFORequestChannel::open_write_pipe(std::string _pipe_name) {

	//if(my_side == SERVER_SIDE)
		create_pipe(_pipe_name);

	wfd = open(_pipe_name.c_str(), O_WRONLY);
	if(wfd < 0) {
		EXITONERROR(_pipe_name);
	}
}

void FIFORequestChannel::open_read_pipe(std::string _pipe_name) {

	//if(my_side == SERVER_SIDE)
		create_pipe(_pipe_name);
	rfd = open(_pipe_name.c_str(), O_RDONLY);
	if(rfd < 0) {
		perror("");
		exit(0);
	}
}

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR FOR CLASS   R e q u e s t C h a n n e l  */
/*--------------------------------------------------------------------------*/

FIFORequestChannel::FIFORequestChannel(const std::string _name, const Side _side) :
side_name((_side == FIFORequestChannel::SERVER_SIDE) ? "SERVER" : "CLIENT"), RequestChannel(_name, _side)
{
	if(_side == SERVER_SIDE) {
		open_write_pipe(pipe_name(WRITE_MODE));
		open_read_pipe(pipe_name(READ_MODE));
	}
	else {
		open_read_pipe(pipe_name(READ_MODE));
		open_write_pipe(pipe_name(WRITE_MODE));
	}
}

FIFORequestChannel::~FIFORequestChannel() {
	close(wfd);
	close(rfd);
	//if(my_side == SERVER_SIDE) {
		remove(pipe_name(READ_MODE).c_str());
		remove(pipe_name(WRITE_MODE).c_str());
	//}
}

const int MAX_MESSAGE = 255;

std::string FIFORequestChannel::cread() {

	char buf [MAX_MESSAGE];
	if(read(rfd, buf, MAX_MESSAGE) <= 0) {
		EXITONERROR("cread");
	}
	std::string s = buf;
	return s;

}

int FIFORequestChannel::cwrite(std::string msg) {
  int size;

	if(msg.size() > MAX_MESSAGE) {
		EXITONERROR("cwrite");
	}

  // perform write
  size = write(wfd, msg.c_str(),(msg.size() + 1));

	if(size < 0) { // msg.size() + 1 to include the NULL byte
		EXITONERROR("cwrite");
	}

  return size;
}


int FIFORequestChannel::read_fd() {
	return rfd;
}

int FIFORequestChannel::write_fd() {
	return wfd;
}
