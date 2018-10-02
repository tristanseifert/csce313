/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:noexpandtab */

/**
 * Implements the prompt where the user can enter commands.
 */
#ifndef PROMPT_H
#define PROMPT_H

#include <string>

class Shell;

class Prompt {
  public:
    Prompt(bool showPrompt);
    virtual ~Prompt();

    void start(void);

  private:
    std::string formatPrompt(void);

  private:
    // should the prompt be shown?
    bool showPrompt = false;
    // prompt string
    std::string prompt = "$USER@$HOST $ ";



    // shell that actually executes stuff
    Shell *shell;
};

#endif
