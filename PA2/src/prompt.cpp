/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

#include "prompt.h"
#include "shell.h"

#include "helpers.h"

#include "inih.h"

#include <iostream>
#include <string>

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>

// define HOST_NAME_MAX if not defined by system (on macOS)
#ifndef HOST_NAME_MAX
  #define HOST_NAME_MAX 255
#endif

/**
 * Creates the prompt class.
 */
Prompt::Prompt(bool _s) : showPrompt(_s) {
  // try to read the kushrc
  this->readKushrc();

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
 * Attempts to read the kushrc file.
 */
void Prompt::readKushrc(void) {
  INIReader reader(".kushrc");

  if(reader.ParseError() < 0) {
    std::cout << "Couldn't read .kushrc, using defaults" << std::endl;
    return;
  }

  // read the prompt
  this->prompt = reader.Get("prompt", "text", this->prompt) + " ";
  this->prompt = this->convertEscapes(this->prompt);
}

/**
 * Converts escape sequences.
 */
std::string Prompt::convertEscapes(std::string &in) {
  std::string escaped = in;

  // replace /e, used for colors
  replaceAll(escaped, "\\e", "\e");

  // newline
  replaceAll(escaped, "\\n", "\n");

  return escaped;
}



/**
 * Formats the prompt string. There are several possible substitutions:
 *
 * - $USER:   Current username
 * - $HOST:   System hostname
 * - $DIR:    Current working directory
 * - $STATUS: Return code of the last command.
 * - $DATE:   Current date
 * - $TIME:   Current time
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
  std::string statusCodeStr = std::to_string(this->lastReturnCode);
  replace(prompt, "$STATUS", statusCodeStr);

  // colored status code
  if(this->lastReturnCode == 0) {
    replace(prompt, "$COLORSTATUS", "\e[32m" + statusCodeStr + "\e[0m");
  } else {
    replace(prompt, "$COLORSTATUS", "\e[31m" + statusCodeStr + "\e[0m");
  }

  // get the time
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);

  // format date string
  const size_t dateBufSz = 64;
  char dateBuf[dateBufSz];
  memset(dateBuf, 0, dateBufSz);

  strftime(dateBuf, dateBufSz, "%x", tm);

  replace(prompt, "$DATE", std::string(dateBuf));

  // format time string
  const size_t timeBufSz = 64;
  char timeBuf[timeBufSz];
  memset(timeBuf, 0, timeBufSz);

  strftime(timeBuf, timeBufSz, "%X", tm);

  replace(prompt, "$TIME", std::string(timeBuf));

  return prompt;
}


/**
 * Starts the prompt's main loop, reading input from the stdin.
 */
void Prompt::start(void) {
  int err = 0;
  bool continueReading = true;

  while(continueReading) {
    if(this->showPrompt) {
      // print the prompt
      std::cout << this->formatPrompt();
    }

    // read input
    std::string command;
    std::getline(std::cin, command);

    // was there a failure reading?
    if(!std::cin) {
      std::cout << "exit" << std::endl;

      // force the exit command so the shell can clean up
      command = "exit";

      continueReading = false;
    }

    // was this line empty?
    if(trim_copy(command).length() == 0) {
      // reset error code and prompt for new input
      this->lastReturnCode = 0;
      continue;
    }

    // add to history
    this->history.push_back(command);

    // pass command to the shell
    err = shell->executeCommandLine(command);

    this->lastReturnCode = err;
  }
}
