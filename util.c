#include <stdlib.h> // for strtol
#include <stdarg.h> // for var args
#include <stdio.h>  // for printf
#include "util.h"

// printf debug
void debug(const char* format, ...) {
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
int parseInt(char* str) {
    return strtol(str, NULL, 10);
}