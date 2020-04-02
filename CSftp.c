#include <stdio.h>
#include <unistd.h> // read()
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h> // memcpy()

#include "dir.h"
#include "usage.h"
#include "defines.h"
#include "util.h"

void waitForClient(int sockFD);

addr getServerAddress(int port)
{
    addr address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    return address;
}

void sendReply(int newSockFD, char* message) {
    write(newSockFD, message, strlen(message));
}

incoming getReply(int newSockFD)
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
        buffer[n] = '\0'; // null termination
        debug("Received message: %s", buffer);
        incoming inc = parseIncoming(buffer);
        debug("Parsed incoming message...");
        if (inc.command == INVALID) {
            debug("Command = INVALID");
        } else {
            debug("Command: %s", inc.readableCmd);
            debug("Argument: %s", inc.argument);
        }
        return inc;
    }
}

void successfulLogin(int newSockFD, int sockFD) {
    while (true) {
        incoming inc = getReply(newSockFD);
        switch (inc.command) {
            case INVALID:
                sendReply(newSockFD, "500 No comprendo\n");
                break;
            case USER:
                sendReply(newSockFD, "530 Can't change from cs317 user\n");
                break;
            case QUIT:
                sendReply(newSockFD, "221 Goodbye.\n");
                return;
        }
    }
   
    // switch (inc.command) {
    
    // }
}

void startLoginSequence(int newSockFD, int sockFD)
{
    debug("startLoginSequence()");
    char *message = "220 Welcome to FTP Server!\n";
    write(newSockFD, message, strlen(message));
    incoming reply = getReply(newSockFD);
    if (reply.command == USER) {
        if (!strcmp(reply.argument, "cs317")) {
            char *loginMessage = "230 Login successful.\n";
            write(newSockFD, loginMessage, strlen(loginMessage));
            debug("Login successful");
            successfulLogin(newSockFD, sockFD);
        } else {
            char *loginMessage = "530 Not logged in\n";
            write(newSockFD, loginMessage, strlen(loginMessage));
            debug("Login not successful");
        };
    } else if (reply.command == QUIT) {
        char *goodbyeMessage = "221 Goodbye.\n";
        write(newSockFD, goodbyeMessage, strlen(goodbyeMessage));
    }
    waitForClient(sockFD);
}

void waitForClient(int sockFD){ 
    // Prepare stuff for accepting client
    addr clientAddress;
    int sizeofClientAddress = sizeof(clientAddress);
    debug("starting accept() ...");
    int newSockFD = accept(sockFD, (struct sockaddr *)&clientAddress, &sizeofClientAddress);
    debug("accept() returned with newSockFD = %d", newSockFD);
    startLoginSequence(newSockFD, sockFD);
    debug("startLoginSequence() returned.");
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
    waitForClient(sockFD);
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
