#include "parser.h"

#include "helpers.h"

#include <string>
#include <vector>

/**
 * Initializes the parser.
 */
Parser::Parser() {

}

/**
 * Cleans up the parser resources.
 */
Parser::~Parser() {

}



/**
 * Splits the string into multiple fragments by pipes.
 */
int Parser::splitByPipes(std::string &command, std::vector<Fragment> &result) {
  // is there a pipe in the string?
  std::size_t found = command.find("|");

  if(found == std::string::npos) {
    // nope, so create a single fragment
    result.push_back(Fragment(command));

    return 0;
  }

  // Go through the string and find pipes
  bool isInQuote = false;
  int lastPipeStart = 0;

  for(int i = 0; i < command.length(); i++) {
    // did we find a quote?
    if(command[i] == '"') {
      isInQuote = !isInQuote;
    }

    // so long as we're not in a quote, check for pipe
    if(!isInQuote && command[i] == '|') {
      // create a substring
      size_t length = (i - lastPipeStart);
      std::string slice = command.substr(lastPipeStart, length);
      trim(slice);

      // create the fragment
      result.push_back(Fragment(slice));

      // save the position
      lastPipeStart = (i + 1);
    }
  }

  // we went through the entire string, still a quote?
  if(isInQuote) {
    return -1;
  }

  // now, create the last fragment slice
  size_t length = (command.length() - lastPipeStart) + 1;
  std::string slice = command.substr(lastPipeStart, length);
  trim(slice);

  result.push_back(Fragment(slice));

  // no errors
  return 0;
}

/**
 * Parses the command line.
 *
 * @return A negative error code, or the number of fragments.
 */
int Parser::parseCommandLine(std::string command, std::vector<Fragment> &result) {
  int err;

  // split it into fragments
  err = this->splitByPipes(command, result);

  if(err < 0) return err;

  // if there's no errors, return the number of fragments
  return result.size();
}
