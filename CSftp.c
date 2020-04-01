#include <stdio.h>
#include <unistd.h> // read()
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h> // memcpy()

#include "dir.h"
#include "usage.h"
#include "util.h"
#include "defines.h"

addr getServerAddress(int port)
{
    addr address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    return address;
}

void getReply(int newSockFD)
{
    debug("getReply()");
    char buffer[BUFFER_SIZE + 1];
    while (true)
    {
        int n = read(newSockFD, buffer, BUFFER_SIZE);
        debug("Received %d bytes.", n);
        if (n <= 0)
        {
            break;
        }
        buffer[n] = 0; // null termination
        debug("Received message: %s", buffer);
        char code[BUFFER_SIZE + 1];
        fgets()
            debug("Parsed code: %s", code);
    }
}

void startLoginSequence(int newSockFD)
{
    debug("startLoginSequence()");
    char *message = "220 Welcome to FTP Server!\n";
    write(newSockFD, message, strlen(message));
    getReply(newSockFD);
}

void startListening(int port)
{
    debug("startListening()");
    debug("creating socket");
    int sockFD = socket(AF_INET, SOCK_STREAM, 0);
    addr serverAddress = getServerAddress(port);
    if (sockFD < 0)
    {
        debug("socket creation failed!");
    }
    debug("created socket! ✓");
    debug("sockFD: %d", sockFD);
    if (bind(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        debug("bind() failed!");
    }
    debug("bind() done! ✓");
    debug("starting listen() ...");
    listen(sockFD, QUEUE_SIZE);
    debug("listen() returned! ✓");
    // Prepare stuff for accepting client
    addr clientAddress;
    int sizeofClientAddress = sizeof(clientAddress);
    debug("starting accept() ...");
    int newSockFD = accept(sockFD, (struct sockaddr *)&clientAddress, &sizeofClientAddress);
    debug("accept() returned with newSockFD = %d", newSockFD);
    startLoginSequence(newSockFD);
    debug("startLoginSequence() returned.");
}

int main(int argc, char *argv[])
{
    // Check the command line arguments
    if (argc != 2)
    {
        usage(argv[0]);
        return -1;
    }

    int port = parseInt(argv[1]);
    if (port < MIN_PORT || port > MAX_PORT)
    {
        usage(argv[0]);
        return -1;
    }
    debug("port: %d", port);
    startListening(port);

    return 0;

    // This is how to call the function in dir.c to get a listing of a
    // directory. It requires a file descriptor, so in your code you would pass
    // in the file descriptor returned for the ftp server's data connection

    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
}
