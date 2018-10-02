/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

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
 * Determines whether any fragment should be backgrounded. This is done by just
 * checking if the fragment's raw text ends with an ampersand.
 */
int Parser::determineBackgrounding(std::vector<Fragment> &result) {
  return 0;
}

/**
 * Parses the command line for some redirection.
 */
int Parser::parseRedirection(std::vector<Fragment> &result) {
  for(auto frag = result.begin(); frag < result.end(); frag++) {
    std::string command = frag->command;

    // is there a redirection character in the command?
    std::size_t found = command.find(">");
    if(found != std::string::npos) goto beach;

    found = command.find("<");
    if(found != std::string::npos) goto beach;

    // couldn't find a redirection character, try the next fragment
    continue;

    // if we get here, we found an IO redirection character
beach: ;
    bool isInQuote = false;

    // go through the command and find the first non-quoted < or >
    for(int i = 0; i < command.length(); i++) {
      // did we find a quote?
      if(command[i] == '"') {
        isInQuote = !isInQuote;
      }

      // IO redirection character?
      if(!isInQuote && (command[i] == '<' || command[i] == '>')) {
        // get substring to the end of the command as filename
        size_t length = (command.length() - i);
        std::string slice = command.substr((i + 1), length);

        // redirecting input?
        if(command[i] == '<') {
          frag->redirectStdin = true;
          frag->stdinFile = trim_copy(slice);
        }
        // redirecting output?
        else if(command[i] == '>') {
          frag->redirectStdout = true;
          frag->stdoutFile = trim_copy(slice);
        }
      }
    }
  }

  return 0;
}

/**
 * Splits the string into multiple fragments by pipes.
 */
int Parser::splitByPipes(std::string &command, std::vector<Fragment> &result) {
  // is there a pipe in the string?
  std::size_t found = command.find("|");

  if(found == std::string::npos) {
    // nope, so create a single fragment
    Fragment f(command);
    f.command = trim_copy(command);
    result.push_back(f);

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

      // create the fragment
      Fragment f(slice);
      f.command = trim_copy(slice);
      result.push_back(f);

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

  Fragment f(slice);
  f.command = trim_copy(slice);
  result.push_back(f);

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

  // extract redirection operators
  err = this->parseRedirection(result);
  if(err < 0) return err;

  // determine whether the fragment will run in the background
  err = this->determineBackgrounding(result);
  if(err < 0) return err;

  // if there's no errors, return the number of fragments
  return result.size();
}



/**
 * Output operator for fragment
 */
std::ostream &operator<<(std::ostream& os, const Parser::Fragment& fragment) {
  os << "Fragment {raw = " << fragment.rawString << "; "
     << "command = " << fragment.command << "; "
     << "background = " << fragment.background << "; "
     << "redirection = {";

  os << "stdout = " << ((fragment.redirectStdout) ? fragment.stdoutFile : "false") << "; ";
  os << "stdin = " << ((fragment.redirectStdin) ? fragment.stdinFile : "false") << "}";

  os << "}";

  return os;
}
