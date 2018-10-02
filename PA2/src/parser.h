/**
 * Parses a command line; it will first attempt to split the input into
 * individual fragments (delimited by pipes) and then extracts IO redirection
 * information, and lastly splits it into individual program arguments.
 */
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

class Parser {
  public:
    Parser();
    virtual ~Parser();

  public:
    class Fragment {
      public:
        Fragment(std::string raw) : rawString(raw) {}

      public:
        std::string rawString;

        bool background = false;
    };

  public:
    int parseCommandLine(std::string command, std::vector<Fragment> &result);

  private:
    int splitByPipes(std::string &command, std::vector<Fragment> &result);
};

#endif
