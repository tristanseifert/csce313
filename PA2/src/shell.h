/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

/**
 * Implements the actual shell, which parses input given by the user, handles
 * the input redirection/piping and backgrounding processes.
 */
#ifndef SHELL_H
#define SHELL_H

#include "parser.h"

#include <string>
#include <vector>

class Shell {
  public:
    Shell();
    virtual ~Shell();

  public:
    int executeCommandLine(std::string command);

  private:
    class Process {
      public:
        // PID of the process
        pid_t pid;
        // the fragment that spawned this process
        Parser::Fragment fragment;
    };

  private:
    void killBackgroundedChildren(void);

    int executeBuiltin(Parser::Fragment &frag);

    int builtinCd(Parser::Fragment &frag);
    int builtinExit(Parser::Fragment &frag);
    int builtinJobs(Parser::Fragment &frag);

    bool isFragmentBuiltin(Parser::Fragment &frag);

    int redirectIO(Parser::Fragment &frag);

    int execFragment(Parser::Fragment &frag);
    int waitForProcess(Process &proc);

    int executeSingle(Parser::Fragment &frag);

    int executeFragmentsWithPipes(std::vector<Parser::Fragment> &fragments);
    int executeFragments(std::vector<Parser::Fragment> &fragments);

  private:
    Parser *parser = nullptr;

    std::vector<Process> backgroundProcesses;
};


#endif
