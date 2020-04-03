#include <stdio.h>
#include <unistd.h> // read()
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h> // memcpy()
#include <pthread.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "dir.h"
#include "usage.h"
#include "defines.h"
#include "util.h"

void waitForClient(int sockFD);

addr getServerAddress(int port)
{
    addr address;
    bzero(&address, sizeof(addr));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    return address;
}

void sendReply(int newSockFD, char* message) {
    write(newSockFD, message, strlen(message));
}

incoming getReply(int clientd)
{
    debug("getReply()");
    char buffer[BUFFER_SIZE + 1];
    while (true)
    {
        int n = read(clientd, buffer, BUFFER_SIZE);
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
            // TODO: Free argument
            debug("Argument: %s", inc.argument);
        }
        return inc;
    }
}

void successfulLogin(int clientd) {
    while (true) {
        incoming inc = getReply(clientd);
        switch (inc.command) {
            case INVALID:
                sendReply(clientd, "500 No comprendo\n");
                break;
            case USER:
                sendReply(clientd, "530 Can't change from cs317 user\n");
                break;
            case QUIT:
                sendReply(clientd, "221 Goodbye.\n");
                return;
            // case NLST:

        }
    }
   
    // switch (inc.command) {
    
    // }
}

void* interact(void* args)
{
    int clientd = *(int*) args;
    debug("start login sequence");
    char *message = "220 Welcome to FTP Server!\n";
    write(clientd, message, strlen(message));
    incoming reply = getReply(clientd);
    if (reply.command == USER) {
        free(reply.argument);
        if (!strcmp(reply.argument, "cs317")) {
            char *loginMessage = "230 Login successful.\n";
            write(clientd, loginMessage, strlen(loginMessage));
            debug("Login successful");
            successfulLogin(clientd);
        } else {
            char *loginMessage = "530 Not logged in\n";
            write(clientd, loginMessage, strlen(loginMessage));
            debug("Login not successful");
        };
    } else if (reply.command == QUIT) {
        free(reply.argument);
        char *goodbyeMessage = "221 Goodbye.\n";
        write(clientd, goodbyeMessage, strlen(goodbyeMessage));
    }
}

void waitForClient(int sockFD){ 
    // Prepare stuff for accepting client
    addr clientAddress;
    socklen_t sizeofClientAddress = sizeof(clientAddress);
    debug("starting accept() ...");
    int clientd = accept(sockFD, (struct sockaddr *)&clientAddress, &sizeofClientAddress);
    if (clientd < 0) {
        return;
    }
    debug("Accepted the client connection from %s:%d.", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
    debug("...with newSockFD = %d", clientd);
    pthread_t thread;
    if (pthread_create(&thread, NULL, interact, &clientd) != 0) {
        perror("Failed to create the thread");
        return;
    }
    pthread_join(thread, NULL);
    debug("Interaction thread has finished.");
}

void startListening(int port)
{
    debug("startListening()");
    debug("creating socket");
    int sockFD = socket(PF_INET, SOCK_STREAM, 0);
    addr serverAddress = getServerAddress(port);
    if (sockFD < 0)
    {
        debug("socket creation failed!");
        exit(-1);
    }
    debug("created socket! ✓");
    debug("sockFD: %d", sockFD);

    // Reuse the address
    int value = 1;
    
    if (setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
    {
        perror("Failed to set the socket option");
    
        exit(-1);
    }

    if (bind(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        debug("bind() failed!");
        exit(-1);
    }
    debug("bind() done! ✓");
    debug("starting listen() ...");
    if (listen(sockFD, QUEUE_SIZE) != 0) {
        perror("Failed to listen for connections");
        exit(-1);
    }
    debug("listen() returned! ✓");
    while (true) {
        waitForClient(sockFD);
    }
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

