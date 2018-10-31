#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <limits.h>

#include "application.h"
#include "link.h"

int main(int argc, char **argv)
{

    //Incorrect argument number
    if (argc < 2)
    {
        printf("Usage:\tnserial SerialPort file File\n\tex: 0 pinguim.gif\n");
        exit(1);
    }

    //Get port
    int portN = atoi(argv[1]);

    //Incorrect port identifier
    if (portN < 0 || portN > 3)
    {
        printf("Usage:\tnserial SerialPort file File\n\tex: 0 pinguim.gif\n");
        exit(1);
    }

    //Proper port format
    char port[11] = "/dev/ttyS";
    strcat(port, argv[1]);

    printf("Using port: %s\n", port);

    //File to be sent (This will be the sender)
    char *file;
    Mode mode;
    if (argc == 3)
    {
        file = argv[2];
        mode = WRITER;
        printf("File provided to be sent: %s\n", file);
    }
    else
    {
        mode = READER;
        printf("No file provided, receiving\n");
    }

    //Setup Link Settings
    connectionSettings(port, mode);

    //File descriptor of connection
    int fd;

    //TODO Open connection
    fd = llopen();

    //TODO Sent file

    //TODO Close connection

    return 0;
}