/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

/**
 * Parses a command line; it will first attempt to split the input into
 * individual fragments (delimited by pipes) and then extracts IO redirection
 * information, and lastly splits it into individual program arguments.
 */
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <iostream>

class Parser {
  public:
    Parser();
    virtual ~Parser();

  public:
    class Fragment {
      friend std::ostream &operator<<(std::ostream&, const Fragment&);

      public:
        Fragment(std::string raw) : rawString(raw) {}

      public:
        // raw fragment string
        std::string rawString;

        // command to actually execute
        std::string command;
        // arguments to the command
        std::vector<std::string> argv;

        // is the process running in the background?
        bool background = false;

        // redirection status
        bool redirectStdout = false;
        std::string stdoutFile;

        bool redirectStdin = false;
        std::string stdinFile;
    };

  public:
    int parseCommandLine(std::string command, std::vector<Fragment> &result);

  private:
    int splitArguments(std::vector<Fragment> &result);
    int determineBackgrounding(std::vector<Fragment> &result);
    int parseRedirection(std::vector<Fragment> &result);
    int splitByPipes(std::string &command, std::vector<Fragment> &result);
};

#endif
