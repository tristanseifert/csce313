/**
 * Implements the actual shell, which parses input given by the user, handles
 * the input redirection/piping and backgrounding processes.
 */
#ifndef SHELL_H
#define SHELL_H

#include <string>

class Shell {
  public:
    Shell();
    virtual ~Shell();

  public:
    int executeCommandLine(std::string command);

  private:
};


#endif