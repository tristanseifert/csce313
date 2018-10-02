/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

#include "prompt.h"
#include "shell.h"

#include "helpers.h"

#include <iostream>
#include <string>

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

// define HOST_NAME_MAX if not defined by system (on macOS)
#ifndef HOST_NAME_MAX
  #define HOST_NAME_MAX 255
#endif

/**
 * Creates the prompt class.
 */
Prompt::Prompt(bool _s) : showPrompt(_s) {
  // set up shell
  this->shell = new Shell();
}

/**
 * Destroys the prompt.
 */
Prompt::~Prompt() {
  // clean up
  delete this->shell;
}



/**
 * Formats the prompt string. There are several possible substitutions:
 *
 * - $USER:   Current username
 * - $HOST:   System hostname
 * - $DIR:    Current working directory
 * - $STATUS: Return code of the last command.
 */
std::string Prompt::formatPrompt(void) {
  int err;

  std::string prompt = this->prompt;

  // substitute user
  struct passwd *pass = getpwuid(getuid());

  if(pass != nullptr) {
    replace(prompt, "$USER", std::string(pass->pw_name));
  } else {
    replace(prompt, "$USER", "UNKNOWN");
  }

  // substitute hostname
  char hostname[HOST_NAME_MAX + 1];
  err = gethostname(hostname, sizeof(hostname));

  if(err == 0) {
    replace(prompt, "$HOST", std::string(hostname));
  } else {
    replace(prompt, "$HOST", "UNKNOWN");
  }

  // substitute cwd
  char *cwd = getcwd(nullptr, 0);

  if(cwd != nullptr) {
    replace(prompt, "$DIR", std::string(cwd));
    free(cwd);
  } else {
    replace(prompt, "$DIR", "UNKNOWN");
  }

  // substitute return code of last command
  replace(prompt, "$STATUS", std::to_string(this->lastReturnCode));

  return prompt;
}


/**
 * Starts the prompt's main loop, reading input from the stdin.
 */
void Prompt::start(void) {
  int err = 0;
  bool continueReading = true;

  while(continueReading) {
    // print the prompt
    std::cout << this->formatPrompt();

    // read input
    std::string command;
    std::getline(std::cin, command);

    // was there a failure reading?
    if(!std::cin) {
      std::cout << "exit" << std::endl;

      continueReading = false;
      break;
    }

    // add to history
    this->history.push_back(command);

    // pass command to the shell
    err = shell->executeCommandLine(command);

    this->lastReturnCode = err;
  }
}
