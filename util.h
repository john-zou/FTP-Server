#ifndef __UTIL_H__
#define __UTIL_H__

enum command
{
    WTF = -1,
    USER = 411 + 0, // 4.1.1
    QUIT = 411 + 1, // 4.1.1
    CWD = 411 + 2,  // 4.1.1
    CDUP = 411 + 3, // 4.1.1
    TYPE = 3113,    // 4.1.1, 3.1.1, 3.1.1.3
    MODE = 341,     // 3.4.1
    STRU = 312 + 1, // 3.1.2, 3.1.2.1
    RETR = 413 + 0, // 4.1.3
    PASV = 411 + 4, // 4.1.1
    NLST = 413 + 1  // 4.1.3
};

struct incoming
{
    enum command command;
    char *arg;
};

typedef struct incoming incoming;

incoming parseIncoming(char *buf);
int parseInt(char *str);
void debug(const char *format, ...);

#endif
