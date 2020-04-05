#include <stdlib.h> // for strtol
#include <stdarg.h> // for var args
#include <stdio.h>  // for printf
#include <string.h> // for strlen
#include <ctype.h>  // for isspace
#include "defines.h"
#include "util.h"

// printf debug
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

// security issue if path starts with / because client can access root
bool isIllegalPath(char *path)
{
    return startsWith(path, "/") || startsWith(path, "..") || startsWith(path, ".") || contains(path, "../");
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

// If valid command matched, fast-forwards next
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

// If valid command found, fast-forwards next
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

void getArgument(incoming *inc, char *buf)
{
    char parsed[BUFFER_SIZE];
    int i = 0;
    while (isspace(buf[i]))
    {
        ++i;
    }
    if (buf[i] == '\0')
    {
        inc->argument = malloc(sizeof(char));
        inc->argument[0] = '\0';
    }
    int j = 0;
    while (!iscntrl(buf[i]))
    {
        parsed[j++] = buf[i++];
    }
    int size = j + 1;
    inc->argument = malloc(sizeof(char) * size);
    for (int k = 0; k < size - 1; ++k)
    {
        inc->argument[k] = parsed[k];
    }
    inc->argument[size - 1] = '\0';
}

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
    if (inc.command == INVALID)
    {
        return inc;
    }
    getArgument(&inc, buf + i);
    return inc;
}

// Returns 0 if the str is not an int
int parseInt(char *str)
{
    return strtol(str, NULL, 10);
}