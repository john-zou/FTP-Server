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
#include <sys/sendfile.h>
#include <sys/stat.h>

#include "dir.h"
#include "usage.h"
#include "defines.h"
#include "util.h"

int datad_local = INVALID_DESCRIPTOR;
int datad = INVALID_DESCRIPTOR;

char currentDirectory[PATH_MAX] = {0}; // initialized to home in main
char homeDirectory[PATH_MAX] = {0};    // initialized to home in main

void waitForClient(int sockFD);
int createSocket(int port);
int getServerIP();

/**
 * Attempts to close any open data sending or acception ports.
 * Called after a message is send on the data connection.
 * **/
void resetDatad()
{
    if (datad_local != INVALID_DESCRIPTOR)
    {
        close(datad_local);
        datad_local = INVALID_DESCRIPTOR;
    }

    if (datad != INVALID_DESCRIPTOR)
    {
        close(datad);
        datad = INVALID_DESCRIPTOR;
    }
}

/**
 * Creates a IPv4 socket address structure with the specified port number
 **/
addr getServerAddress(int port)
{
    addr address;
    bzero(&address, sizeof(addr));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    return address;
}

/**
 * Send a reply to the control connection.
 * Print debug message if sending fails.
 **/
void sendReply(int clientd, char *message)
{
    debug("Sending... %s", message);
    int sendStatus = send(clientd, message, strlen(message), 0);
    if (sendStatus == -1)
    {
        debug("Send failed.");
    }
}

/**
 * Read a reply from the control connection using a buffer.
**/
incoming getReply(int clientd)
{
    debug("getReply()");
    char buffer[BUFFER_SIZE + 1];
    while (true)
    {
        int n = read(clientd, buffer, BUFFER_SIZE);
        debug("Received %d bytes.", n);
        // Handle unexpected quit
        if (n <= 0)
        {
            return parseIncoming("QUIT");
        }
        buffer[n] = '\0';
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
            debug("Argument: %s", inc.argument);
        }
        return inc;
    }
}

/**
 * Opens a data connection listening and sending port for the client. 
 * Attempts to create a socket for a random port and listen on it.
 * Fetches the IP address of the server and sends the random port and IP to the client.
 **/
int PASVhelper(int clientd)
{
    // close exisiting data connection if there is one
    resetDatad();
    // Try to create a new data connection
    int port;
    while (true)
    {
        port = (rand() % 64512) + 1024;
        int fd = createSocket(port);
        if (fd != INVALID_DESCRIPTOR)
        {
            datad_local = fd;
            break;
        }
    }
    // ok to use datad for pasv
    debug("Established data socket. Port: %d", port);

    // Get the IP address to advertise
    addr sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    unsigned int sockaddrlen = sizeof(sockaddr);
    getsockname(datad_local, (struct sockaddr *)&sockaddr, &sockaddrlen);
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
    sendReply(clientd, ipResponse);

    // accept datad_local and get datad
    addr clientDataAddress;
    socklen_t sizeofClientDataAddress = sizeof(clientDataAddress);
    debug("starting accept() on data port ...");

    while (datad == INVALID_DESCRIPTOR)
    {
        datad = accept(datad_local, (struct sockaddr *)&clientDataAddress, &sizeofClientDataAddress);
        debug("PASV accept returned with datad: %d", datad);
    }
    debug("Accepted the client connection from %s:%d.", inet_ntoa(clientDataAddress.sin_addr), ntohs(clientDataAddress.sin_port));
    debug("...with datad = %d", datad);
    return 0;
}

/**
 * Transfers data to the client on the port returned by PASV.
 * Opens a local fd and sends using sendfile kernel call.
 * Responds with errors if file not found or if unallowed/ non-existent filenames are requested.
 **/
int RETRhelper(incoming *inc, int clientd)
{
    if (datad == INVALID_DESCRIPTOR)
    {
        sendReply(clientd, "425 Do PASV first.\r\n");
    }
    else
    {
        char *filename = inc->argument;
        // What if required file is /passwords.txt?? following the constraints on CWD
        if (isIllegalPath(filename))
        {
            sendReply(clientd, "550 Illegal path\r\n");
        }
        else
        {
            // 1. open the file
            FILE *file = fopen(filename, "rb");
            if (file == NULL)
            {
                debug("file: %s not found", filename);
                resetDatad();
                sendReply(clientd, "550 Failed to open file.\r\n");
            }
            else
            {
                // 2. send "150 Opening binary data mode data connection for \r\n"
                // always in binary mode regardless of TYPE according to https://piazza.com/class/k4szj0ldhzy433?cid=616
                debug("Preparing to send: %s", filename);
                sendReply(clientd, "150 Opening binary data mode data connection.\r\n");

                // 3. send file through datad

                struct stat fileStat;
                fstat(fileno(file), &fileStat);

                if (sendfile(datad, fileno(file), NULL, fileStat.st_size) == -1)
                {
                    // Inform the client on the control connection that an error has been encountered
                    resetDatad();
                    sendReply(clientd, "451 Transfer aborted.\r\n");
                }
                else
                {
                    resetDatad();
                    // 4. send "226 Transfer complete."
                    sendReply(clientd, "226 Transfer complete.\r\n");
                }
                fclose(file);
            }
        }
        resetDatad();
    }
    return 0;
}

/**
 * Performs the equivalent of CWD ../
 * Disallows going up from the home directory.
 **/
void CDUPhelper(int clientd)
{
    // don't go up if already in home
    if (strcmp(currentDirectory, homeDirectory) == 0)
    {
        sendReply(clientd, "550 Insufficient Permissions to go up in the directory.\r\n");
    }
    else
    {
        if (chdir("../") == 0)
        {
            memset(currentDirectory, 0, PATH_MAX);
            getcwd(currentDirectory, PATH_MAX);
            debug("Changed dir to: %s", currentDirectory);
            sendReply(clientd, "250 Directory successfully changed.\r\n");
        }
        else
        {
            debug("Can't change dir to ../");
            debug("Current dir: %s", currentDirectory);
            sendReply(clientd, "550 Failed to change directory.\r\n");
        }
    }
}

/**
 * Changes directory to the given argument.
 * Disallows directories starting with ../, ./ or (.. , .) as per the spec.
 * Additionaly disallows cd /
 **/
void CWDhelper(incoming *inc, int clientd)
{
    char *path = inc->argument;
    // check illegal path
    debug("Trying to cwd to %s", path);
    if (!isIllegalPath(path))
    {
        debug("legal path detected :^)");
        if (startsWith(path, "/"))
        {
            // 1. Make a copy of path
            char path_copy[strlen(path) + 1];
            strcpy(path_copy, path);
            // 2. Realloc path to PATH_MAX
            free(inc->argument);
            inc->argument = malloc(PATH_MAX);
            path = inc->argument;
            // 3. Copy home dir in path
            memset(path, 0, PATH_MAX);
            size_t homeLen = strlen(homeDirectory);
            memcpy(path, homeDirectory, homeLen);
            // 4. Append client path to home directory in "path"
            memcpy(path + homeLen, path_copy, strlen(path_copy) + 1); // +1 for null terminator
        }

        if (chdir(path) == 0)
        {
            memset(currentDirectory, 0, PATH_MAX);
            getcwd(currentDirectory, PATH_MAX);
            debug("Now current directory is: %s", currentDirectory);
            sendReply(clientd, "250 Directory successfully changed.\r\n");
            return;
        }
        else
        {
            sendReply(clientd, "550 Failed to change directory.\r\n");
        }
    }
    else
    {
        debug("Illegal/ inaccessible path detected :^(");
        sendReply(clientd, "550 Illegal path.\r\n");
    }
}

/**
 * The main interaction loop with the client control connection
 * 
 **/
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
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            sendReply(clientd, "530 Can't change from cs317 user\r\n");
            break;
        case QUIT:
            if (inc.numArguments != NO_ARGUMENTS)
            {
                sendReply(clientd, INA501);
                break;
            }
            chdir(homeDirectory);
            memset(currentDirectory, 0, PATH_MAX);
            strcpy(currentDirectory, homeDirectory);
            sendReply(clientd, "221 Goodbye.\r\n");
            resetDatad();
            free(inc.argument);
            debug("Freed %p\n", inc.argument);
            return;
        case PASV:
            if (inc.numArguments != NO_ARGUMENTS)
            {
                sendReply(clientd, INA501);
                break;
            }
            PASVhelper(clientd);
            break;
        case TYPE:
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            // only supoort ascii (A) and binary (B) type
            if (strcmp("A", inc.argument) == 0)
                sendReply(clientd, "200 Switching to ASCII mode.\r\n");
            else if (strcmp("I", inc.argument) == 0)
                sendReply(clientd, "200 Switching to Binary mode.\r\n");
            else if (strcmp("E", inc.argument) == 0 || strcmp("L", inc.argument) == 0)
                sendReply(clientd, CNI504);
            else
                sendReply(clientd, "501 Unrecognised TYPE argument.\r\n");
            break;
        case MODE:
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            // only support stream mode
            if (strcmp("S", inc.argument) == 0)
                sendReply(clientd, "200 Mode set to S.\r\n");
            else if (strcmp("B", inc.argument) == 0 || strcmp("C", inc.argument) == 0)
                sendReply(clientd, CNI504);
            else
                sendReply(clientd, "501 Unrecognised MODE argument.\r\n");
            break;
        case STRU:
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            // only support F type
            if (strcmp("F", inc.argument) == 0)
                sendReply(clientd, "200 Structure set to F.\r\n");
            else if (strcmp("R", inc.argument) == 0 || strcmp("P", inc.argument))
                sendReply(clientd, CNI504);
            else
                sendReply(clientd, "501 Unrecognised STRU argument.\r\n");
            break;
        case NLST:
            if (inc.numArguments != NO_ARGUMENTS)
            {
                sendReply(clientd, INA501);
                break;
            }
            // 1. Ensure there is a  datad connection
            if (datad == INVALID_DESCRIPTOR)
            {
                sendReply(clientd, "425 Do PASV first.\r\n");
                break;
            }
            // 2. Send "150 Here comes the directory listing.\r\n" through clientd
            sendReply(clientd, "150 Here comes the directory listing.\r\n");
            // 3. Do listFiles with current directory
            listFiles(datad, currentDirectory);
            // 4. Close data connection
            resetDatad();
            // 5. Send "226 Directory send OK.\r\n" through clientd
            sendReply(clientd, "226 Directory send OK.\r\n");
            break;
        case CWD:
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            CWDhelper(&inc, clientd);
            break;
        case CDUP:
            if (inc.numArguments != NO_ARGUMENTS)
            {
                sendReply(clientd, INA501);
                break;
            }
            CDUPhelper(clientd);
            break;
        case RETR:
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
                break;
            }
            RETRhelper(&inc, clientd);
            break;
        }
        free(inc.argument);
        debug("Freed %p\n", inc.argument);
    }
}
/**
 * Interacts with the client after the initial connection is setup.
 * Logs in a client if the username is cs317.
 * Sends a login fail error otherwise and keeps listening.
 * Returns if the client quits before logging in.
 */
void *interact(void *args)
{
    int clientd = *(int *)args;
    debug("start login sequence");
    char *message = "220 Welcome to FTP Server!\r\n";
    sendReply(clientd, message);
    while (true)
    {
        incoming inc = getReply(clientd);
        if (inc.command == USER)
        {
            if (inc.numArguments != ONE_ARGUMENT)
            {
                sendReply(clientd, INA501);
            }
            // if client is cs317
            else if (!strcmp(inc.argument, "cs317"))
            {
                free(inc.argument);
                debug("Freed %p\n", inc.argument);

                char *loginMessage = "230 Login successful.\r\n";
                sendReply(clientd, loginMessage);
                debug("Login successful");
                interactPostLogin(clientd);
                return NULL;
            }
            else
            {
                char *loginMessage = "530 Not logged in\r\n";
                sendReply(clientd, loginMessage);
                debug("Login not successful");
            };
        }
        else if (inc.command == QUIT)
        {
            if (inc.numArguments != NO_ARGUMENTS)
            {
                sendReply(clientd, INA501);
                break;
            }
            char *goodbyeMessage = "221 Goodbye.\r\n";
            free(inc.argument);
            debug("Freed %p\n", inc.argument);

            sendReply(clientd, goodbyeMessage);
            return NULL;
        }
        else
        {
            sendReply(clientd, "500 Unsupported.\r\n");
        }
        free(inc.argument);
        debug("Freed %p\n", inc.argument);
    }
}

/**
 * Handles incoming client connections and creates a new thread to interact with the client.
 * Closes a client connection after the client quits or errors occur during interaction.
 */
void waitForClient(int sockFD)
{
    resetDatad();
    // Prepare stuff for accepting client
    addr clientAddress;
    socklen_t sizeofClientAddress = sizeof(clientAddress);
    debug("starting accept() ...");
    int clientd = INVALID_DESCRIPTOR;

    while (clientd == INVALID_DESCRIPTOR)
    {
        clientd = accept(sockFD, (struct sockaddr *)&clientAddress, &sizeofClientAddress);
        debug("Accepting control connection on clientd: %d", clientd);
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
    debug("Closing clientd socket #%d", clientd);
    close(clientd);
}

/**
 * Creates a socket for a specified port, bind and listen on it.
 * Used for both data and control.
 **/
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

/**
 * Starts the server on the user-defined port.
 * Argument: port: the user-defined port
 **/
void startServer(int port)
{
    debug("startServer()");
    int sockFD = createSocket(port);
    if (sockFD == INVALID_DESCRIPTOR)
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
}

/** 
 *   Get the "public" server ip address
 *   Reference -  https://stackoverflow.com/questions/4139405/how-can-i-get-to-know-the-ip-address-for-interfaces-in-c
 *   Returns the IP address and port number as an int
 **/
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
