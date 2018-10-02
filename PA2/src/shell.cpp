#include "shell.h"

#include <iostream>
#include <string>

/**
 * Initializes the shell.
 */
Shell::Shell() {

}

/**
 * Terminates any remaining processes and cleans up the system state.
 */
Shell::~Shell() {

}



/**
 * Executes a command line given by the user.
 */
int Shell::executeCommandLine(std::string command) {
  std::cout << "\treceived command: " << command << std::endl;

  return -1;
}
