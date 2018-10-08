/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

#include "shell.h"

#include "parser.h"
#include "helpers.h"
#include "config.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <cstdio>
#include <cstring>

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
  int killed = 0;

  // exit if we don't have children
  if(this->backgroundProcesses.empty()) return;

  for(auto it = this->backgroundProcesses.begin(); it < this->backgroundProcesses.end(); it++) {
    // is the process still alive?
    err = kill(it->pid, 0);

    if(err != 0) continue;

    // kill the process
    err = kill(it->pid, SIGKILL);
    killed++;

    if(err != 0) {
      std::cout << "\tCouldn't kill background process " << it->pid << ": "
        << strerror(errno) << std::endl;
    }
  }

  // print a message indicating how many background processes were killed
  if(killed > 0) {
    std::cout << "Killed " << this->backgroundProcesses.size()
      << " background processes" << std::endl;
    }
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
  if(frag.argv.size() == 1) {
    return 0;
  }

  // get the path to change to
  std::string path = frag.argv[1];

  // if the path is just a dash, go up one directory
  if(trim_copy(path) == "-") {
    path = "..";
  }

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
 * Executes multiple fragments by piping between them with ¥enpipes
 *
 * The first fragment has its stdin connected to the console (or redirected,)
 * and the last fragment has its stdout connected to the console (or redirected)
 * while the remaining processes take their input from the output of the process
 * before them.
 */
int Shell::executeFragmentsWithPipes(std::vector<Parser::Fragment> &fragments) {
  int err, status;
  pid_t waitFor;

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

    // push that process into the list
    if(child != 0) {
      Process p;
      p.pid = child;
      p.fragment = *it;

      processes.push_back(p);
    }


    // connect the ¥enpipes in the child
    if(child == 0) {
      // redirect stdin for the first process if needed
      if(it == fragments.begin() && it->redirectStdin) {
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
      // redirect stdout for the first process if needed
      else if(it == (fragments.end() - 1) && it->redirectStdout) {
        int newStdout = open(it->stdoutFile.c_str(), (O_RDWR | O_TRUNC | O_CREAT), 0644);

        // handle errors
        if(newStdout == -1) {
          std::cout << "Couldn't open " << it->stdoutFile << ": "
            << strerror(errno) << std::endl;

          return errno;
        }

        // replace stdin
        err = dup2(newStdout, STDOUT_FILENO);

        if(err == -1) {
          std::cout << "dup2() failed: " << strerror(errno) << std::endl;
          return errno;
        }
      }

      // connect the ¥enpipes as needed
      {
        // connect stdin
        int stdinIndex = (i - 1);

        if(stdinIndex >= 0) {
          // std::cout << "stdin for " << i << " is " << stdinIndex << std::endl;

          err = dup2(pipes[stdinIndex][0], STDIN_FILENO);

          if(err == -1) {
            std::cout << "dup2() failed: " << strerror(errno) << std::endl;
            return errno;
          }
        }

        // connect stdout for all but the last process
        int stdOutIndex = (i - 0);

        if(i < (fragments.size() - 1)) {
          // std::cout << "stdout for " << i << " is " << stdOutIndex << std::endl;

          err = dup2(pipes[stdOutIndex][1], STDOUT_FILENO);

          if(err == -1) {
            std::cout << "dup2() failed: " << strerror(errno) << std::endl;
            return errno;
          }
        }
      }

      // exec the process
      err = this->execFragment(*it);

      // if we get down here, there was an error
      std::cout << "Couldn't exec(): " << strerror(errno) << " (" << errno << ")"
        << std::endl;
      goto cleanup;
    }
  }

  // wait for the last process in the pipe to complete
  // status = this->waitForProcess(processes.back());

  // wait for any process to complete
  err = wait(&status);

  if(err == -1) {
    std::cout << "Error waiting: " << strerror(errno) << std::endl;
  }

  // extract the exit code
  if(WIFEXITED(status)) {
    status = WEXITSTATUS(status);
  }

  // close all pipes
  for(int i = 0; i < fragments.size(); i++) {
    // make sure to close both ends
    for(int j = 0; j < 2; j++) {
      err = close(pipes[i][j]);
      if(err) {
        std::cout << "Couldn't close pipe: " << strerror(errno) << std::endl;
      }
    }
  }

  // kill all the processes we spawned, ignoring any errors
  sleep(1);

  for(auto it = processes.begin(); it < processes.end(); it++) {
    kill(it->pid, SIGKILL);
  }

  // return status code
  return status;

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
 * Waits for the given process to finish, then returns its return code or
 * some other value indicating the error.
 */
int Shell::waitForProcess(Process &proc) {
  int status = 0;
  pid_t result = waitpid(proc.pid, &status, 0);

  // did the process exit?
  if(WIFEXITED(status)) {
    // return with its exit code
    return WEXITSTATUS(status);
  }
  // did the process receive a signal?
  else if(WIFSIGNALED(status)) {
    std::cout << "Child " << proc.pid << " exited on signal "
      << WTERMSIG(status) << std::endl;

    // check for coredump
    if(WCOREDUMP(status)) {
      std::cout << "Core dumped." << std::endl;
    }

    // return the signal
    return -WTERMSIG(status);
  }

  // shouldn't get down here, so return raw status
  return status;
}

/**
 * Executes the given fragment.
 *
 * @note This function will NOT return unless there is an error.
 */
int Shell::execFragment(Parser::Fragment &frag) {
  int err;

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
  return err;
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

    // execute the process
    err = this->execFragment(frag);

    // exit if we get down here
    exit(errno);
  }

  // run only in parent
  if(child != 0) {
    // create a process object
    Process p;

    p.pid = child;
    p.fragment = frag;

    // if task isn't backgrounded, wait for it to exit
    if(!frag.background) {
      return this->waitForProcess(p);
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

  // reap any zombies
  err = wait3(NULL, WNOHANG, NULL);

  if(err == -1) {
    // ignore ENOCHILD
    if(errno != ECHILD) {
      std::cout << "wait3() failed: " << strerror(errno) << std::endl;
    }
  }

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
