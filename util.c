#include <stdlib.h> // for strtol
#include <stdarg.h> // for var args
#include <stdio.h>  // for printf
#include <string.h> // for strlen
#include <ctype.h>  // for isspace
#include "defines.h"
#include "util.h"

/**
 *   Helper function that prints a formatted debug message and appends a newline after it
 *   This function only does anything if "make debug" is used, which defines DEBUG.
 **/
void debug(const char *format, ...)
{
#ifdef DEBUG
    va_list va;
    va_start(va, format);
    printf("»»--DEBUG--► ");
    vfprintf(stdout, format, va);
    printf("\n");
#endif
    return;
}

/**
 *   Returns true (1) if path starts with pattern, and false (0) if it doesn't.
 *   Arguments:  path: null-terminated char array
 *               pattern: null-terminated char array
 **/
bool startsWith(char *path, char *pattern)
{
    int len = strlen(pattern);
    if (strlen(path) < len)
    {
        return false;
    }
    for (int i = 0; i < len; ++i)
    {
        if (path[i] != pattern[i])
        {
            return false;
        }
    }
    return true;
}

/**
 *   Returns true (1) if path contains pattern, and false (0) if it doesn't
 *   Arguments:  path: null-terminated char array
 *               pattern: null-terminated char array
 **/
bool contains(char *path, char *pattern)
{
    int pathLen = strlen(path);
    int len = strlen(pattern);
    if (pathLen < len)
    {
        return false;
    }
    for (int i = 0; i < pathLen - len + 1; ++i)
    {
        if (startsWith(path + i, pattern))
        {
            return true;
        }
    }
    return false;
}

/**
 *    Returns true (1) if path is considered dangerous, and false (0) if it isn't
 *    Arguments:  path: null-terminated char array
 **/
bool isIllegalPath(char *path)
{
    return !strcmp(path, "..") || startsWith(path, "./") || contains(path, "../");
}

struct cmd COMMAND_STRING[] = {
    {"USER ", USER},
    {"QUIT ", QUIT},
    {"CWD  ", CWD},
    {"CDUP ", CDUP},
    {"TYPE ", TYPE},
    {"MODE ", MODE},
    {"STRU ", STRU},
    {"RETR ", RETR},
    {"PASV ", PASV},
    {"NLST ", NLST},
};

const int COMMAND_STRING_LEN = 10;

/**
 *   Returns true (1) if candidate starts with pattern, and sets the next offset to help parse the argument
 *   Returns false (0) if there is no match, and does not change next
 *   Arguments:  candidate: null-terminated char array
 *               pattern: null-terminated char array
 *               next: pointer to offset
 **/
bool matchCommand(char *candidate, char *pattern, int *next)
{
    int c = 0, p = 0;
    while (pattern[p] != ' ')
    {
        if (candidate[c++] != pattern[p++])
        {
            return false;
        }
    }
    if (!isspace(candidate[c]))
    {
        if (iscntrl(candidate[c]))
        {
            *next += p;
            return true;
        }
        else
        {
            return false;
        }
    }
    *next += p + 1;
    return true;
}

/**
 *   Parses a char buffer (buf) into a command (enum), readable string of the command (readableCmd)
 *   and saves it into inc.
 *   Increments the next offset to help parse the argument, if a valid command is found.
 *   
 *   Arguments:  inc: pointer to structure containing parsed message from client
 *               buf: null-terminated char buffer of message from client
 *               next: pointer to offset
 **/
void getCommand(incoming *inc, char *buf, int *next)
{
    char *start = buf + *next;
    for (int i = 0; i < COMMAND_STRING_LEN; ++i)
    {
        if (matchCommand(start, COMMAND_STRING[i].str, next))
        {
            inc->command = COMMAND_STRING[i].command;
            for (int j = 0; j < 5; ++j)
            {
                inc->readableCmd[j] = COMMAND_STRING[i].str[j];
            }
            inc->readableCmd[5] = '\0';
            return;
        }
    }
    inc->command = INVALID;
}

/**
 *    Saves a char buffer into inc as its argument and saves the number of arguments (separated by whitespace).
 *   
 *    Arguments:  inc: pointer to structure containing parsed message from client
 *                buf: null-terminated char buffer of message from client
 **/
void getArgument(incoming *inc, char *buf)
{
    inc->numArguments = NO_ARGUMENTS;
    char parsed[BUFFER_SIZE];
    int i = 0;
    while (isspace(buf[i]))
    {
        ++i;
    }
    if (buf[i] == '\0')
    {
        inc->argument = malloc(sizeof(char));
        debug("stupid malloced 1 byte @ %p\n", inc->argument);
        inc->argument[0] = '\0';
        return;
    }
    int j = 0;
    bool noSpace = true;
    bool stopCopying = false;
    while (!iscntrl(buf[i]))
    {
        if (isspace(buf[i]))
        {
            stopCopying = true;
        }
        if (j > 0 && isspace(buf[i - 1]) && !isspace(buf[i]))
        {
            noSpace = false;
        }
        if (!stopCopying)
        {
            parsed[j++] = buf[i++];
        } else {
            i++;
        }
    }
    int size = j + 1;
    if (j > 0)
    {
        inc->numArguments = ONE_ARGUMENT;
    }
    if (!noSpace)
    {
        inc->numArguments = MORE_THAN_ONE_ARGUMENTS;
    }
    if (j == 0)
    {
        inc->numArguments = NO_ARGUMENTS;
    }
    inc->argument = malloc(sizeof(char) * size);
    debug("malloced %d bytes @ %p\n", size, inc->argument);
    for (int k = 0; k < size - 1; ++k)
    {
        inc->argument[k] = parsed[k];
    }
    inc->argument[size - 1] = '\0';
}

/**
 *    Parses the message from the client into a structure, returning it.
 *
 *    Arguments: buf: null-terminated char buffer
 *    Return: an incoming struct, containing command (enum), readable string of command, and argument
 **/
incoming parseIncoming(char *buf)
{
    int len = strlen(buf);
    int i = 0, j = 0;
    while (isspace(buf[i]))
    {
        ++i;
    }
    incoming inc;
    getCommand(&inc, buf, &i);
    getArgument(&inc, buf + i);
    return inc;
}

/**
 *   Allows the programmer to pretend they are using JavaScript for a second
 *   
 *   Argument: str: a null-terminated string that is probably a number
 *   Returns: the integer representation, or 0 if it's not a number
 **/
int parseInt(char *str)
{
    return strtol(str, NULL, 10);
}