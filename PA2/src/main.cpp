#include <iostream>

#include <unistd.h>



#include "prompt.h"

/**
 * Prints the help string.
 */
static void printHelp(char *programName) {
  std::cout << "usage: " << programName << " [-t] "
    << "" << std::endl << std::endl;
  std::cout << "PARAMETERS" << std::endl;
  std::cout << "-h: Shows this help message." << std::endl;
  std::cout << "-t: Disables printing the shell prompt." << std::endl;
}

/**
 * Entry point for the shell.
 */
int main(int argc, char *argv[]) {
  bool showPrompt = true;

  // parse command line options
  int c;
  while((c = getopt(argc, argv, "ht")) != -1) {
    switch(c) {
      // disable prompt
      case 't':
        showPrompt = false;
        break;

      // show help
      case 'h':
        printHelp(argv[0]);
        exit(0);
      // error
      case '?':
        printHelp(argv[0]);
        exit(-1);

      // shouldn't get there
      default:
        break;
    }
  }

  // allocate the prompt to start the shell
  Prompt *p = new Prompt(showPrompt);

  p->start();

  // done
  delete p;
}
