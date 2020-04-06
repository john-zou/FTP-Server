#ifndef __DEFINES_H__
#define __DEFINES_H__

typedef int bool;
#define true 1
#define false 0

typedef struct sockaddr_in addr;

#define MIN_PORT 1024
#define MAX_PORT 65535
#define BUFFER_SIZE 1024
#define QUEUE_SIZE 1
#define USERNAME "cs317"
#define LOOPBACK_ADDR "127.0.0.1"
#define INVALID_DESCRIPTOR -1

#endif