#ifndef _METHODS_H
#define _METHODS_H

#include <unistd.h>

#define PRINT_PROMPT(x) printf("%s",#x); fflush(stdout);
#define MAX_CMD_LENGTH 2048
#define MAX_ARGUMENTS 512
#define MAX_PROCESSES 1000
#define TRUE 1
#define FALSE 0
#define TRACE 0
#define	FLUSH fflush(stdout)

/* Prototypes */
void printTitle(); //prints title
void clearBuffer(); //clears buffer after fgets operation
void cleanBG(int signo); //waits for child for cleanup
void terminateFG(int signo); //terminate foreground process
int executeCommand(char** argv, int len, int bg, pid_t* chld); //executes non-builtin commands
int redirectIO(char** argv, char* fileIn, char* fileOut, int len, int bg, pid_t* chld); //redirects input and output in foreground and background
int redirectInput(char** argv, char* fileName, int len, int bg, pid_t* chld); //redirects input in foreground and background
int redirectOutput(char** argv, char* fileName, int len, int bg, pid_t* chld); //redirects output in foreground and background

#endif
