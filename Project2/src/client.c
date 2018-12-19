#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <ctype.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"

#define ARG_SIZE 100

#define BUF_SIZE 500

struct Arguments
{
    char user[ARG_SIZE];
    char password[ARG_SIZE];
    char host[ARG_SIZE];
    char path[ARG_SIZE];
    char fileName[ARG_SIZE];
} arguments;

//Parse Arguments
int parseArguments(int argc, char **argv)
{

    //Wrong argument size
    if (argc < 2)
        return -1;

    //Raw string
    char *raw = argv[1];

    //Global index
    int i = 6;

    //Detect username and password
    char c;
    int found = 0;
    do
    {
        c = raw[i];
        if (c == '@')
        {
            found = 1;
            break;
        }
        i++;
    } while (c != '\0');

    //Reset counter
    i = 6;

    //Local index
    int li = 0;

    //Processing state
    int state = 0;

    //If didnt find user name assume anonymous and skip states
    if (found == 0)
    {
        printf("No user credentials provided, assuming anonymous login\n");
        state = 2;
        strcpy(arguments.user, "anonymous");
        strcpy(arguments.password, "anonymous");
    }

    //Process
    do
    {
        c = raw[i];
        switch (state)
        {
        case 0:
            if (c == ':')
            {
                arguments.user[li] = '\0';
                state++;
                li = 0;
                break;
            }
            arguments.user[li] = c;
            li++;
            break;
        case 1:
            if (c == '@')
            {
                arguments.password[li] = '\0';
                state++;
                li = 0;
                break;
            }
            arguments.password[li] = c;
            li++;
            break;
        case 2:
            if (c == '/')
            {
                arguments.host[li] = '\0';
                state++;
                li = 0;
                break;
            }
            arguments.host[li] = c;
            li++;
            break;
        case 3:
            if (c == '\0')
            {
                arguments.path[li] = '\0';
                state++;
                break;
            }
            arguments.path[li] = c;
            li++;
            break;
        default:
            break;
        }
        i++;
    } while (c != '\0');

    if (state < 4)
        return -1;

    //Get filename
    int iPath;
    int pathSize = strlen(arguments.path);
    int iFilename = 0;

    //Go through path and write to filename but clear when finding a '/'
    //In the end we will have the last piece of the path, the file name
    for (iPath = 0; iPath < pathSize; iPath++)
    {
        //Reset index
        if (arguments.path[iPath] == '/')
            iFilename = 0;
        //Write to string
        else
        {
            arguments.fileName[iFilename] = arguments.path[iPath];
            iFilename++;
        }
    }

    //Close name
    arguments.fileName[iFilename] = '\0';

    return 0;
}

//Get ip address according to the host's name
struct hostent *getip(char *host)
{
    struct hostent *h;

    if ((h = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }

    return h;
}

//Get response code
int response(int socket, char *code)
{
    int state = 0;
    int i = 0;
    char c;

    while (state != 3)
    {
        if (read(socket, &c, 1) == -1)
            return -1;

        printf("%c", c);

        switch (state)
        {
        //3 digit number
        case 0:
            if (c == ' ')
            {
                if (i != 3)
                {
                    printf("Error receiving response code\n");
                    return -1;
                }
                i = 0;
                state = 1;
            }
            else
            {
                if (c == '-')
                {
                    state = 2;
                    i = 0;
                }
                else
                {
                    if (isdigit(c))
                    {
                        code[i] = c;
                        i++;
                    }
                }
            }
            break;
        //Read until end of line
        case 1:
            if (c == '\n')
            {
                state = 3;
            }
            break;
        //Multi line response
        case 2:
            if (c == code[i])
            {
                i++;
            }
            else
            {
                if (i == 3 && c == ' ')
                {
                    state = 1;
                }
                else
                {
                    if (i == 3 && c == '-')
                    {
                        i = 0;
                    }
                }
            }
            break;
        }
    }

    return 0;
}

//Get file
void getFile(int fd, char *filename)
{
    //Open file
    FILE *file = fopen((char *)filename, "w");

    //Write in chunks
    char bufSocket[BUF_SIZE];
    int bytes;
    while ((bytes = read(fd, bufSocket, BUF_SIZE)) > 0)
        bytes = fwrite(bufSocket, bytes, 1, file);

    //Close file
    fclose(file);

    printf("File downloaded\n");
}

int sendCommand(int socketfd, char cmdBase[], char commandContent[], char *fileName, int socketfdClient)
{
    char code[3];
    int type = 0;

    //Send command
    if (write(socketfd, cmdBase, strlen(cmdBase)) == -1)
        return -1;
    if (write(socketfd, commandContent, strlen(commandContent)) == -1)
        return -1;
    if (write(socketfd, "\n", 1) == -1)
        return -1;

    while (1)
    {
        //Read response code
        response(socketfd, code);
        type = code[0] - '0';

        switch (type)
        {
        //Other response
        case 1:
            if (strcmp(cmdBase, "retr ") == 0)
            {
                getFile(socketfdClient, fileName);
                break;
            }
            response(socketfd, code);
            break;
        //Accepted
        case 2:
            return 0;
        //Incomplete information
        case 3:
            return 1;
        //Retry
        case 4:
            if (write(socketfd, cmdBase, strlen(cmdBase)) == -1)
                return -1;
            if (write(socketfd, commandContent, strlen(commandContent)) == -1)
                return -1;
            if (write(socketfd, "\r\n", 2) == -1)
                return -1;
            break;
        //Failed
        case 5:
            printf("Command refused, not retrying\n");
            close(socketfd);
            exit(-1);
        }
    }
}

//Get server port
int getPort(int socketfd)
{    
    
    //Only need the last 2
    char fifth[4];
    char sixth[4];

    char c;
    int index = 0;
    int state = 0;
    while (state != 7)
    {
        if (read(socketfd, &c, 1) == -1)
            return -1;

        printf("%c", c);

        switch (state)
        {
        case 0:
            if (c == ' ')
            {
                if (index != 3)
                {
                    printf("Error receiving response code in getPort()\n");
                    return -1;
                }
                index = 0;
                state = 1;
            }
            else
                index++;
            break;
        case 5:
            if (c == ',')
            {
                index = 0;
                state++;
            }
            else
            {
                fifth[index] = c;
                index++;
            }
            break;
        case 6:
            if (c == ')')
                state++;
            else
            {
                sixth[index] = c;
                index++;
            }
            break;
        default:
            if (c == ',')
                state++;
            break;
        }
    }

    int fifthVal = atoi(fifth);
    int sixthVal = atoi(sixth);
    int port = fifthVal * 256 + sixthVal;
    return port;
}

// Example
// ./download ftp://epiz_23144137:j0WozA4EXa7abK@ftpupload.net/htdocs/pasta1/pasta2/test.txt
// ./download ftp://anonymous:anonymous@ftp.up.pt/pub/CTAN/timestamp
// ./download ftp://anonymous:anonymous@ftp.up.pt/CentOS/2.1/readme.txt

int main(int argc, char **argv)
{

    //Parse arguments
    if (parseArguments(argc, argv) < 0)
    {
        printf("\nInvalid arguments");
        printf("\nUsage example:");
        printf(" download ftp://[<user>:<password>@]<host>/<url-path>\n");
    }

    //Print information to be used
    printf("Username: %s\n", arguments.user);
    printf("Password: %s\n", arguments.password);
    printf("Host: %s\n", arguments.host);
    printf("Path: %s\n", arguments.path);
    printf("File: %s\n", arguments.fileName);

    //Get IP
    struct hostent *h = getip(arguments.host);

    //Print IP
    printf("\nIP Address: %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

    //Handle address
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));
    server_addr.sin_port = htons(SERVER_PORT);

    //Open TCP socket
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(0);
    }

    //Connect to Server
    if (connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(0);
    }

    //Read response
    char code[3];
    if (response(socketfd, code) < 0)
    {
        perror("response()");
        exit(0);
    }

    //Confirm response
    if (code[0] == '2')
    {
        printf("Connection Estabilished\n");
    }

    //Send username
    printf("Sending Username\n");
    int socketfdClient = -1;
    sendCommand(socketfd, "user ", arguments.user, NULL, socketfdClient);

    //Send password
    printf("Sending Password\n");
    sendCommand(socketfd, "pass ", arguments.password, NULL, socketfdClient);

    //Get server port to be used
    printf("Setting passive mode\n");
    if (write(socketfd, "pasv\n", 5) == -1)
        return -1;
    int serverPort = getPort(socketfd);
    printf("\nPort received: %d", serverPort);

    //Handle address
    struct sockaddr_in server_addr_client;
    bzero((char *)&server_addr_client, sizeof(server_addr_client));
    server_addr_client.sin_family = AF_INET;
    server_addr_client.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));
    server_addr_client.sin_port = htons(serverPort);

    //Open TCP socket to that port
    if ((socketfdClient = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(0);
    }

    //Connect to server
    if (connect(socketfdClient, (struct sockaddr *)&server_addr_client, sizeof(server_addr_client)) < 0)
    {
        perror("connect()");
        exit(0);
    }

    //Send retr and get file
    sendCommand(socketfd, "retr ", arguments.path, arguments.fileName, socketfdClient);

    //Close everything
    close(socketfdClient);
    close(socketfd);

    return 0;
}
