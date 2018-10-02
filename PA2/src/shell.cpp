/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

#include "shell.h"

#include "parser.h"
#include "config.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <unistd.h>

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
 * Determines whether the given fragment will execute a built-in function or
 * whether it needs to be executed with exec()
 */
bool Shell::isFragmentBuiltin(Parser::Fragment &frag) {
  // change directory (cd)
  if(frag.command == "cd") return true;
  // exit shell
  if(frag.command == "exit") return true;

  // not a builtin
  return false;
}

/**
 * Changes the current working directory.
 */
int Shell::builtinCd(Parser::Fragment &frag) {
  int err;

  // if no arguments, do nothing
  if(frag.argv.size() == 0) {
    return 0;
  }

  // get the path to change to
  std::string path = frag.argv[0];

  // change to it and handle errors
  err = chdir(path.c_str());

  if(err != 0) {
    std::cout << "Couldn't cd: " << strerror(errno) << std::endl;
  }

  return err;
}

/**
 * Exits the shell.
 */
int Shell::builtinExit(Parser::Fragment &frag) {
  std::cout << "Goodbye!" << std::endl;

  exit(0);
}

/**
 * Executes a built-in function.
 */
int Shell::executeBuiltin(Parser::Fragment &frag) {
  // Change directory (cd)?
  if(frag.command == "cd") {
    return this->builtinCd(frag);
  }
  // exit shell (exit)?
  else if(frag.command == "exit") {
    return this->builtinExit(frag);
  }

  // shouldn't get here
  throw std::runtime_error("Invalid builtin");
}



/**
 * Executes multiple fragments by piping between them.
 */
int Shell::executeFragmentsWithPipes(std::vector<Parser::Fragment> &fragments) {
  // TODO: implement
  return -1;
}

/**
 * Executes one or more fragments.
 *
 * If there is only one fragment, it is directly executed.
 *
 * If there are more than one fragments, the output of the first fragment is
 * redirected to the input of the second, and so forth.
 *
 * Returns the exit code of the rightmost fragment.
 */
int Shell::executeFragments(std::vector<Parser::Fragment> &fragments) {
  // is there only one fragment? if so, execute it directly
  if(fragments.size() == 1) {
    Parser::Fragment frag = fragments[0];

    if(this->isFragmentBuiltin(frag)) {
      return this->executeBuiltin(frag);
    } else {
      // TODO: implement
    }
  }
  // otherwise, execute with piping
  else {
    return this->executeFragmentsWithPipes(fragments);
  }

  return -1;
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

#if DBG_LOGGING
  // DEBUG: output fragments
  for(auto i = fragments.begin(); i < fragments.end(); i++) {
    std::cout << "\t" << *i << std::endl;
  }
#endif

  // execute command
  err = this->executeFragments(fragments);
  return err;
}
