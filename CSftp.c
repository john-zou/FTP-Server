#include <stdio.h>
#include <unistd.h> // read()
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h> // memcpy()
#include <pthread.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <linux/limits.h> // for PATH_MAX

#include "dir.h"
#include "usage.h"
#include "defines.h"
#include "util.h"

int datad = NO_DATA_CONNECTION;
char currentDirectory[PATH_MAX] = {0}; // initialized to home in main
char homeDirectory[PATH_MAX] = {0};    // initialized to home in main

void waitForClient(int sockFD);
int createSocket(int port);
int getServerIP();

addr getServerAddress(int port)
{
    addr address;
    bzero(&address, sizeof(addr));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    return address;
}

// Returns -1 if failed
int sendReply(int clientd, char *message)
{
    return send(clientd, message, strlen(message), 0);
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
        if (inc.command == INVALID)
        {
            debug("Command = INVALID");
        }
        else
        {
            debug("Command: %s", inc.readableCmd);
            // TODO: Free argument
            debug("Argument: %s", inc.argument);
        }
        return inc;
    }
}

void interactPostLogin(int clientd)
{
    while (true)
    {
        incoming inc = getReply(clientd);
        switch (inc.command)
        {
        case INVALID:
            sendReply(clientd, "500 No comprendo\r\n");
            break;
        case USER:
            sendReply(clientd, "530 Can't change from cs317 user\r\n");
            break;
        case QUIT:
            // TODO: close both control and data
            sendReply(clientd, "221 Goodbye.\r\n");
            return;
        case PASV:
            // close exisiting data connection
            if (datad != NO_DATA_CONNECTION)
            {
                close(datad);
                datad = NO_DATA_CONNECTION;
            }
            // Try to create a new data connection
            int port;
            while (true)
            {
                port = (rand() % 64512) + 1024;
                int fd = createSocket(port);
                if (fd != -1)
                {
                    datad = fd;
                    break;
                }
            }
            // ok to use datad for pasv
            debug("Established data socket. Port: %d", port);

            // Get the IP address to advertise
            addr sockaddr;
            memset(&sockaddr, 0, sizeof(sockaddr));
            unsigned int sockaddrlen = sizeof(sockaddr);
            getsockname(datad, (struct sockaddr *)&sockaddr, &sockaddrlen);
            int serverIP = getServerIP();

            char ipResponse[200];
            // memset(&ipResponse, 0, sizeof(ipResponse));
            int mask = (1 << 8) - 1;
            int messageLen = sprintf(ipResponse, "227 Entering passive mode (%d,%d,%d,%d,%d,%d)\r\n",
                                     serverIP & mask,
                                     (serverIP >> 8) & mask,
                                     (serverIP >> 16) & mask,
                                     (serverIP >> 24) & mask,
                                     port / (1 << 8),
                                     port % (1 << 8));
            ipResponse[messageLen] = '\0';
            if (sendReply(clientd, ipResponse) == -1)
            {
                debug("sendReply failed!");
                break;
            }
            break;
        case TYPE:
            if (strcmp("A", inc.argument) == 0)
            {
                // ok image type
                sendReply(clientd, "200 Switching to ASCII mode.\r\n");
            }
            else if (strcmp("I", inc.argument) == 0)
            {
                // we do not support binary
                sendReply(clientd, "200 Switching to Binary mode.\r\n");
            }
            else
            {
                // unrecognised type command
                sendReply(clientd, "500 Unrecognised TYPE command.\r\n");
            }
            break;
        case MODE:
            if (strcmp("S", inc.argument) == 0)
            {
                sendReply(clientd, "200 Mode set to S.\r\n");
            }
            else
            {
                sendReply(clientd, "500 Unrecognised MODE argument.\r\n");
            }
            break;
        case STRU:
            if (strcmp("F", inc.argument) == 0)
            {
                sendReply(clientd, "200 Structure set to F.\r\n");
            }
            else
            {
                sendReply(clientd, "500 Unrecognised STRU argument.\r\n");
            }
            break;
        case NLST:
            // 1. Ensure there is a  datad connection
            if (datad == NO_DATA_CONNECTION)
            {
                sendReply(clientd, "425 Do PASV first.\r\n");
                break;
            }
            else
            {
                // 2. Send "150 Here comes the directory listing.\r\n" through clientd
                sendReply(clientd, "150 Here comes the directory listing.\r\n");
                // 3. Do listFiles(datad, ) with current directory
                listFiles(datad, currentDirectory);
                // 4. Send "226 Directory send OK.\r\n" through clientd
                sendReply(clientd, "226 Directory send OK.\r\n");
            }
            break;
        }
    }
}

void *interact(void *args)
{
    int clientd = *(int *)args;
    debug("start login sequence");
    char *message = "220 Welcome to FTP Server!\r\n";
    write(clientd, message, strlen(message));
    while (true)
    {
        incoming reply = getReply(clientd);
        if (reply.command == USER)
        {
            if (!strcmp(reply.argument, "cs317"))
            {
                char *loginMessage = "230 Login successful.\r\n";
                write(clientd, loginMessage, strlen(loginMessage));
                debug("Login successful");
                interactPostLogin(clientd);
                return NULL;
            }
            else
            {
                char *loginMessage = "530 Not logged in\r\n";
                write(clientd, loginMessage, strlen(loginMessage));
                debug("Login not successful");
            };
        }
        else if (reply.command == QUIT)
        {
            char *goodbyeMessage = "221 Goodbye.\r\n";
            write(clientd, goodbyeMessage, strlen(goodbyeMessage));
            return NULL;
        }
    }
}

void waitForClient(int sockFD)
{
    datad = NO_DATA_CONNECTION; // reset datad
    // Prepare stuff for accepting client
    addr clientAddress;
    socklen_t sizeofClientAddress = sizeof(clientAddress);
    debug("starting accept() ...");
    int clientd = accept(sockFD, (struct sockaddr *)&clientAddress, &sizeofClientAddress);
    if (clientd < 0)
    {
        return;
    }
    debug("Accepted the client connection from %s:%d.", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
    debug("...with newSockFD = %d", clientd);
    pthread_t thread;
    if (pthread_create(&thread, NULL, interact, &clientd) != 0)
    {
        perror("Failed to create the thread");
        return;
    }
    pthread_join(thread, NULL);
    debug("Interaction thread has finished.");
}

int createSocket(int port)
{
    debug("creating socket");
    int sockFD = socket(PF_INET, SOCK_STREAM, 0);
    addr serverAddress = getServerAddress(port);
    if (sockFD < 0)
    {
        debug("socket creation failed!");
        return -1;
    }
    debug("created socket! ✓");
    debug("sockFD: %d", sockFD);

    // Reuse the address
    int value = 1;

    if (setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
    {
        perror("Failed to set the socket option");

        return -1;
    }

    if (bind(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        debug("bind() failed!");
        return -1;
    }
    debug("bind() done! ✓");
    debug("starting listen() ...");
    if (listen(sockFD, QUEUE_SIZE) != 0)
    {
        perror("Failed to listen for connections");
        return -1;
    }
    debug("listen() returned! ✓");
    return sockFD;
}

void startServer(int port)
{
    debug("startServer()");
    int sockFD = createSocket(port);
    if (sockFD == -1)
    {
        perror("failed to create initial socket");
        exit(-1);
    }

    while (true)
    {
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

    getcwd(homeDirectory, PATH_MAX);
    getcwd(currentDirectory, PATH_MAX);
    debug("Home directory: %s", homeDirectory);

    int port = parseInt(argv[1]);
    if (port < MIN_PORT || port > MAX_PORT)
    {
        usage(argv[0]);
        return -1;
    }
    debug("port: %d", port);
    startServer(port);

    return 0;

    // This is how to call the function in dir.c to get a listing of a
    // directory. It requires a file descriptor, so in your code you would pass
    // in the file descriptor returned for the ftp server's data connection

    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
}

// Get the "public" server ip address
// reference -  https://stackoverflow.com/questions/4139405/how-can-i-get-to-know-the-ip-address-for-interfaces-in-c
int getServerIP()
{
    struct ifaddrs *ifap, *ifa;
    addr *sa;
    char *address;

    getifaddrs(&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            sa = (addr *)ifa->ifa_addr;
            // readable format for debug
            address = inet_ntoa(sa->sin_addr);
            // reject localhost
            if (strcmp(LOOPBACK_ADDR, address) != 0)
            {
                debug("Interface: %s\tAddress: %s\n", ifa->ifa_name, address);
                int ip = sa->sin_addr.s_addr;
                freeifaddrs(ifap);
                return ip;
            }
        }
    }
    debug("Something went horribly wrong with getServerIP");
    freeifaddrs(ifap);
    return -1;
}
