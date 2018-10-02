/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

/**
 * Implements the prompt where the user can enter commands.
 */
#ifndef PROMPT_H
#define PROMPT_H

#include <string>
#include <vector>

class Shell;

class Prompt {
  public:
    Prompt(bool showPrompt);
    virtual ~Prompt();

    void start(void);

  private:
    std::string formatPrompt(void);

  private:
    // history of commands
    std::vector<std::string> history;

    // should the prompt be shown?
    bool showPrompt = false;
    // prompt string
    std::string prompt = "[$STATUS] $USER@$HOST - $DIR $ ";

    // last return code
    int lastReturnCode = 0;



    // shell that actually executes stuff
    Shell *shell;
};

#endif
