#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "util.h"
#include "dir.h"
#include "usage.h"

#define MIN_PORT 1024
#define MAX_PORT 65535
#define BUFFER_SIZE 1024

// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

int main(int argc, char *argv[]) {
    // Check the command line arguments
    if (argc != 2) {
      usage(argv[0]);
      return -1;
    }

    int port = parseInt(argv[1]);
    if (port < MIN_PORT || port > MAX_PORT) {
      usage(argv[0]);
      return -1;
    }
    debug("port: %d", port);

    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection
    
    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
}
