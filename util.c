#include <stdlib.h> // for strtol
#include <stdarg.h> // for var args
#include <stdio.h>  // for printf
#include <string.h> // for strlen
#include <ctype.h>  // for isspace
#include "util.h"

int getCommand(incoming *inc, char *buf)
{
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
    getCommand(&inc, buf + i);
    // TODO continue
}

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

// Returns 0 if the str is not an int
int parseInt(char *str)
{
    return strtol(str, NULL, 10);
}