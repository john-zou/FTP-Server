#ifndef __UTIL_H__
#define __UTIL_H__

enum command
{
    INVALID = -1,
    USER = 1, // 4.1.1
    QUIT = 2, // 4.1.1
    CWD = 3,  // 4.1.1
    CDUP = 4, // 4.1.1
    TYPE = 5, // 4.1.1, 3.1.1, 3.1.1.3
    MODE = 6, // 3.4.1
    STRU = 7, // 3.1.2, 3.1.2.1
    RETR = 8, // 4.1.3
    PASV = 9, // 4.1.1
    NLST = 10 // 4.1.3
};

struct cmd
{
    char str[5];
    enum command command;
};

struct incoming
{
    enum command command;
    char readableCmd[6];
    char *argument;
    int numArguments;
};

typedef struct incoming incoming;

bool startsWith(char *path, char *pattern);
bool isIllegalPath(char *path);
incoming parseIncoming(char *buf);
int parseInt(char *str);
void debug(const char *format, ...);

#endif
