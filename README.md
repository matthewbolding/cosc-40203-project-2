# Project 2: hfsh

## Spring 2022
## Due February 3, 2022 by 15:30

---

## Objective

The purpose of this assignment is to get everyone up to speed on system programming and to gain some familiarity with system calls and process creation.  A secondary goal is to use some additional software development tools provided in the UNIX environment.  In this assignment you are to implement a UNIX/Linux shell program.  A shell is simply a CLI program that conveniently allows you to run other programs.  Read up on your favorite shell (such as bash, csh, tcsh, sh, ...) to see what it does.  This project is based on the shell project in your textbook in Chapter 3, and a custom project of my own design.

---

## Getting Started

By now you have downloaded the project repository to `lovelace.cs.tcu.edu` or your local machine.

Decide if you would like to do this project in C or C++ (suggested is C++ since it has the C++ standard template library).  You need to create a `Makefile` symbolic link in your project subdirectory.  This link will point to the language specific makefile you wish use for this project.  If you choose C then type

```text 
ln -s Makefile_c Makefile
```

If you choose C++ then type 
```text
ln -s Makefile_cpp Makefile
```
From there on you will edit either the hfsh.c or hfsh.cpp file respectively.  To compile your project, simply type `make` at the command prompt.

The extra files that are supplied or get created perform the scanning functionality for a input line into your shell.  This project uses a compiler tool called `flex` that will scan the input line and separate it into tokens - character strings.  Specifically the file `scan.l` describes how to split the line.  It may look cryptic, but for now, you do not have to make any changes to this file.

Whether you choose to write your program in C or C++, a separate program template have been provided for you (you pick).  Inside each of these programs is a simple main function that calls the `gettoks()` function and returns an array of variably length c-strings (null terminated) where each array element is one token (character string).  The array of strings is then printed out in a simple for-loop.  You can use the array of strings (tokens) to determine the name of the program to run, command line arguments or options, and special characters such as pipes and I/O redirection symbols.

```text
linux$ make
linux$ ./hfsh
[...you type a series of internal and external commands...]
myexit
[...myexit displays a table of signal counts...]
linux$
```

---

## Requirements

- (5 points)  Write your own signal handler functions for the signals `SIGINT`, `SIGTSTP`, `SIGHUP`, and `SIGQUIT`.  This some of the first executable statements in the `main()` function.  Each time these signals are asynchronously sent to your shell process, incremented a separate counter for each.  When your shell process exits (via the `myexit` command) print a small table showing the type of signal and number of times each was handled.

- (5 points)  Create a dynamic shell prompt that changes either every time the user presses [ENTER] to execute a command, or changes as a result of the last command.  Examples include showing the current date and time, showing the current working subdirectory, command number, etc.  Static prompts will result in loss of points.  Creating / formatting a new shell prompt (string) should be a separate function.

_**Note:**_ make sure your processes properly exit and are signaled back to the calling/creating process.

- (20 points) If the command entered is an external command, then your shell will create a child process with or without arguments.  The child process will run in the foreground by default.  If the last token on the command line is the ampersand character (&), then your program must fork a child process and execute in the background.  If it is a background process, your shell will print the next prompt and wait for the user to enter the next command.  You should have separate parent and child functions in your program.

    - Example foreground: `linux$ cal 2016`
	- Example background: `linux$ sleep 15 &` 

_**Note:**_ make sure your processes properly exit and are signaled back to the calling/creating process.

- (10 points)  A command, with or without arguments that uses file redirection. 

    - Example: `linux$ ls -l > out.txt` 
	- Example: `linux$ sort < in.txt`
	- Example: `linux$ sort -r < in.txt > out.txt`

- (10 points) A command with or without arguments that pipes the output of the first process into the input of a second process.
    - Example: `linux$ ls | grep README`
    - asdfafd

- (20 points) Implement a command history feature.  The basic functionality of the history feature should work exactly as is does in the bash shell however you do not have to store the commands to a file.  Instead, your history data structure will be in memory and it will get reset each time you execute you shell program.  Your shell history command will be named `myhist`.  This should be written as separate functions.  Experiment with the current bash shell and reverse engineer its features into your shell.

- (10 points) Implement a new internal command called `forweb` which takes the name of a subdirectory _dirname_ and sets "other" permissions to `o+r` for files and `o+rx` for subdirectories in the file hierarchy rooted at _dirname_.  This should be written as a separate function.
	
- (10 points)  Implement a new internal command called `nls` that is similar to the `ls` command but that, by default, displays regular files and directories separately.  This should be written as separate functions.  If the file is a subdirectory, the text should be blue.  If the file is a regular file, it should be gray.  If the file is a link, it should be red.  If the file is an executable, it should be green.
	
- (10 points)  Implement a new internal command called of your choice and design.  Your new internal command must be a part of the file system services or process management services.  It must not be a current shell command or system command.  Your new Linux command must use system library functions.  Be creative.  Be unique.	 Send me an email or stop by my office to discuss your internal command before you implement it (required).

_**Note:**_ there are a total of 5 internal commands: `myexit`, `myhist`, `forweb`, `nls`, and one of your own design.  

---

## Submitting Your Project
You will exclusively use github classroom to upload your project.

---

## Assessment and Grading
- This is an individual assignment.  
- Write a well-documented, well-structured C/C++ program using procedures and functions.  Programs that are all in the ``main function'' or have excessively long functions will result in loss of points. 
- You may use the STL in your solution.  
- I will test your program on `lovelace.cs.tcu.edu`. 
- This project is worth 100 points.
- This project is due February 3 by 15:30.
