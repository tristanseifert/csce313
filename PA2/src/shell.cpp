#include "shell.h"

#include "parser.h"

#include <iostream>
#include <string>

/**
 * Initializes the shell.
 */
Shell::Shell() {
  // create the parser and configure it
  this->parser = new Parser();
}

/**
 * Terminates any remaining processes and cleans up the system state.
 */
Shell::~Shell() {
  // clean up resources
  delete this->parser;
}



/**
 * Executes a command line given by the user.
 */
int Shell::executeCommandLine(std::string command) {
  int err;

  // parse the command line
  std::vector<Parser::Fragment> fragments;

  err = this->parser->parseCommandLine(command, fragments);

  if(err <= -1) {
    // there is a parsing error :(
    std::cout << "? Syntax error" << std::endl;
    return err;
  }

  // DEBUG: output fragments
  for(auto i = fragments.begin(); i < fragments.end(); i++) {
    std::cout << "Fragment: " << i->rawString << std::endl;
  }

  // execute the command
  std::cout << "\treceived command: " << command << std::endl;

  return -1;
}
