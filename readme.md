# Smallsh -- Fischer Jemison

This is a shell program created for the Operating Systems I class at Oregon State University. It features signal handling, process management, outside program management, and more.

To compile, run `make`. If you don't have unix make, then you can open Makefile and run the command from the default target. This program compiles with GCC using C89. It is guaranteed to run on the version of CentOS used on the OSU Engineering flip server; your mileage may vary on different linux distributions. In theory, there is no danger of fork-bombing your computer while you are using this program, but use at your own caution. It would probably behoove you to run `ps` after you exit the program to make sure there are no stray processes to clean up.

Once compiled, run `./smallsh` to start the shell.

You can run most bash programs by typing the program name. Programs can be run in the background of the shell by placing an `&` at the end of the command. You can view the exit status of the last command by running `status`, and exit the program by running `exit`.
