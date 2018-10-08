# KUSH, the Kool Underdeveloped Shell
This implements the second programming assignment, a basic shell.

## Compilation
Use the included makefile by executing `make`. Any standards-compliant C++11 compiler should be able to compile the shell without issues.

The built shell is in `build/kush`.

## Features
Basic shell features, such as pipes and IO redirection are implemented with a bash-style syntax.

### Customization
To customize the shell, place a file named `.kushrc` in the directory you invoke the shell from. (The project ships with a basic `.kushrc` in this directory, so if you invoke the shell as `./build/kush` it will pull in those settings.)

## Dependencies
This project uses [inih](https://github.com/jtilly/inih) to parse the `.kushrc` file.
