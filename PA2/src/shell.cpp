/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

#include "shell.h"

#include "parser.h"
#include "config.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

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
  // kill children
  this->killBackgroundedChildren();

  // clean up resources
  delete this->parser;
}



/**
 * Kills all backgrounded child processes.
 */
void Shell::killBackgroundedChildren(void) {
  int err;

  // exit if we don't have children
  if(this->backgroundProcesses.empty()) return;

  for(auto it = this->backgroundProcesses.begin(); it < this->backgroundProcesses.end(); it++) {
    // kill the process
    err = kill(it->pid, SIGKILL);

    if(err != 0) {
      std::cout << "\tCouldn't kill background process " << it->pid << ": "
        << strerror(errno) << std::endl;
    }
  }

  // print a message indicating how many background processes were killed
  std::cout << "Killed " << this->backgroundProcesses.size()
    << " background processes" << std::endl;
}



/**
 * Determines whether the given fragment will execute a built-in function or
 * whether it needs to be executed with exec()
 */
bool Shell::isFragmentBuiltin(Parser::Fragment &frag) {
  // change directory
  if(frag.command == "cd") return true;
  // exit shell
  if(frag.command == "exit") return true;
  // show jobs
  if(frag.command == "jobs") return true;

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

  // kill children
  this->killBackgroundedChildren();

  // exit
  exit(0);
}

/**
 * Lists all background processes.
 */
int Shell::builtinJobs(Parser::Fragment &frag) {
  int err;

  // iterate over all background jobs
  int i = 1;
  for(auto it = this->backgroundProcesses.begin(); it < this->backgroundProcesses.end(); it++) {
    std::cout << "[" << i++ << "]\t";

    // print PID
    std::cout << it->pid << "\t";

    // is the process running?
    errno = 0;
    err = kill(it->pid, 0);

    if(err == 0) {
      std::cout << "running\t\t";
    } else {
      std::cout << "stopped\t\t";
    }

    // print the command line
    std::cout << it->fragment.rawString << std::endl;
  }

  return 0;
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
  // show backgrounded processes (jobs)?
  else if(frag.command == "jobs") {
    return this->builtinJobs(frag);
  }

  // shouldn't get here
  throw std::runtime_error("Invalid builtin");
}



/**
 * Executes multiple fragments by piping between them.
 *
 * The first fragment has its stdin connected to the console (or redirected,)
 * and the last fragment has its stdout connected to the console (or redirected)
 * while the remaining processes take their input from the output of the process
 * before them.
 */
int Shell::executeFragmentsWithPipes(std::vector<Parser::Fragment> &fragments) {
  int err;

  // is there more fragments than we can allocate pipes for?
  if(fragments.size() > MAX_PIPES) {
    std::cout << "Couldn't set up processes: max pipes exceeded" << std::endl;
    return -1;
  }

  // Create pipes (one per fragment; [0] is stdin, [1] is stdout)
  int pipes[MAX_PIPES][2];

  for(int i = 0; i < fragments.size(); i++) {
    // attempt to create pipes
    err = pipe(pipes[i]);

    if(err != 0) {
      // XXX: probably should clean up pipes but lol
      std::cout << "Couldn't create pipe: " << strerror(errno) << std::endl;
      return -1;
    }
  }

  // keep track of all the processes we spawn
  std::vector<Process> processes;

  int i = 0;

  for(auto it = fragments.begin(); it < fragments.end(); it++, i++) {
    // fork the process to create a child
    pid_t child = fork();

    if(child == -1) {
      std::cout << "fork() failed: " << strerror(errno) << std::endl;
      goto cleanup;
    }

    // is this the first process? (redirect stdin if needed)
    if(it == fragments.begin()) {
      // does stdin need to be redirected?
      if(it->redirectStdin) {
        int newStdin = open(it->stdinFile.c_str(), O_RDONLY);

        // handle errors
        if(newStdin == -1) {
          std::cout << "Couldn't open " << it->stdinFile << ": "
            << strerror(errno) << std::endl;

          return errno;
        }

        // replace stdin
        err = dup2(newStdin, STDIN_FILENO);

        if(err == -1) {
          std::cout << "dup2() failed: " << strerror(errno) << std::endl;
          return errno;
        }
      }
    }
    // or is it the last process? (redirect stdout if needed)
    else if(it == (fragments.end() - 1)) {
      int newStdin = open(it->stdoutFile.c_str(), (O_RDWR | O_TRUNC | O_CREAT), 0644);

      // handle errors
      if(newStdin == -1) {
        std::cout << "Couldn't open " << it->stdoutFile << ": "
          << strerror(errno) << std::endl;

        return errno;
      }

      // replace stdin
      err = dup2(newStdin, STDOUT_FILENO);

      if(err == -1) {
        std::cout << "dup2() failed: " << strerror(errno) << std::endl;
        return errno;
      }
    }
    // connect the pipes as needed
    else {
      // TODO: kush
    }

    // build argv
    const size_t argvSize = sizeof(char *) * (it->argv.size() + 1);
    char **argv = static_cast<char **>(malloc(argvSize));

    int i = 0;
    for(auto arg = it->argv.begin(); arg < it->argv.end(); arg++) {
      argv[i++] = const_cast<char *>(arg->c_str());
    }

    // null terminate argv!
    argv[i] = nullptr;

    // run exec
    err = execvp(it->command.c_str(), static_cast<char * const *>(argv));

    // if we get down here, there was an error
    std::cout << "Couldn't exec(): " << strerror(errno) << " (" << errno << ")"
      << std::endl;
    goto cleanup;
  }

  // TODO: implement
  return -1;

  // drop down here if an error occurrs during process creation
cleanup: ;
  // kill all processes we spawned so far
  for(auto it = processes.begin(); it < processes.end(); it++) {
    // kill it
    err = kill(it->pid, SIGKILL);

    // handle errors… we're basically fucked at this point but try to continue
    if(err != 0) {
      std::cout << "kill() failed: " << strerror(errno) << std::endl;
      return -1;
    }
  }

  // return -1 bc fuck it
  return -1;
}



/**
 * Redirects the standard input/outputs as needed.
 *
 * @note This should only be called from within the child, after fork().
 */
int Shell::redirectIO(Parser::Fragment &frag) {
  int err;

  // does stdin need to be redirected?
  if(frag.redirectStdin) {
    int newStdin = open(frag.stdinFile.c_str(), O_RDONLY);

    // handle errors
    if(newStdin == -1) {
      std::cout << "Couldn't open " << frag.stdinFile << ": "
        << strerror(errno) << std::endl;

      return errno;
    }

    // replace stdin
    err = dup2(newStdin, STDIN_FILENO);

    if(err == -1) {
      std::cout << "dup2() failed: " << strerror(errno) << std::endl;
      return errno;
    }
  }

  // does stdout need to be redirected?
  if(frag.redirectStdout) {
    int newStdin = open(frag.stdoutFile.c_str(), (O_RDWR | O_TRUNC | O_CREAT), 0644);

    // handle errors
    if(newStdin == -1) {
      std::cout << "Couldn't open " << frag.stdoutFile << ": "
        << strerror(errno) << std::endl;

      return errno;
    }

    // replace stdin
    err = dup2(newStdin, STDOUT_FILENO);

    if(err == -1) {
      std::cout << "dup2() failed: " << strerror(errno) << std::endl;
      return errno;
    }
  }

  // success if we get down here
  return 0;
}

/**
 * Executes a single process.
 */
int Shell::executeSingle(Parser::Fragment &frag) {
  int err;

  // fork to create a new process
  pid_t child = fork();

  if(child == -1) {
    std::cout << "fork() failed: " << strerror(errno) << std::endl;
    return child;
  }

  // run process only in child
  if(child == 0) {
    // perform IO redirection
    err = this->redirectIO(frag);

    if(err != 0) {
      return err;
    }

    // build argv
    const size_t argvSize = sizeof(char *) * (frag.argv.size() + 1);
    char **argv = static_cast<char **>(malloc(argvSize));

    int i = 0;
    for(auto arg = frag.argv.begin(); arg < frag.argv.end(); arg++) {
      argv[i++] = const_cast<char *>(arg->c_str());
    }

    // null terminate argv!
    argv[i] = nullptr;

    // run exec
    err = execvp(frag.command.c_str(), static_cast<char * const *>(argv));

    // if we get down here, there was an error
    std::cout << "Couldn't exec(): " << strerror(errno) << " (" << errno << ")"
      << std::endl;
    exit(errno);
  }

  // run only in parent
  if(child != 0) {
    // create a process object
    Process p;

    p.pid = child;
    p.fragment = frag;

    // if task isn't backgrounded, wait for it
    if(!frag.background) {
      int status = 0;
      pid_t result = waitpid(child, &status, 0);

      // did the process exit?
      if(WIFEXITED(status)) {
        // return with its exit code
        return WEXITSTATUS(status);
      }
      // did the process receive a signal?
      else if(WIFSIGNALED(status)) {
        std::cout << "Child " << child << " exited on signal "
          << WTERMSIG(status) << std::endl;

        // check for coredump
        if(WCOREDUMP(status)) {
          std::cout << "Core dumped." << std::endl;
        }

        return -2;
      }
    }
    // it is backgrounded, just add it to the background processes list
    else {
      this->backgroundProcesses.push_back(p);

      // assume success
      return 0;
    }
  }

  // we shouldn't get here…
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
      return this->executeSingle(frag);
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
