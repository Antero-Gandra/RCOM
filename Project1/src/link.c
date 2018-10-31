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

#include "link.h"
#include "alarm.h"

Settings *settings;

const int FLAG = 0x7E;
const int A = 0x03;
const int ESCAPE = 0x7D;

//Setup connection settings
void connectionSettings(char *port, Mode mode)
{
    settings = (Settings *)malloc(sizeof(Settings));

    strcpy(settings->port, port);
    settings->mode = mode;
    settings->baudRate = B38400;
    settings->messageDataMaxSize = 512;
    settings->timeout = 3;
    settings->numTries = 3;
}

//Prepares and sends command to fd
void sendCommand(int fd, Control com)
{

    //Command size
    const int commandMaxSize = 5 * sizeof(char);

    //Prepare command
    unsigned char* command = malloc(commandMaxSize);

    command[0] = FLAG;
    command[1] = A;
    command[2] = com;
    if (com == C_REJ || com == C_RR)
        command[2] |= (settings->ns << 7);
    command[3] = command[1] ^ command[2];
    command[4] = FLAG;

    //Stuffing
    int commandSize = stuff(&command, (commandMaxSize));

    //Send command
    if (write(fd, command, commandSize) != (commandMaxSize))
    {
        printf("ERROR: Could not write %s command.\n", command);
    }

    //Free command
    free(command);
    
}

//Stuffing
int stuff(unsigned char **buf, int bufSize)
{
    int newBufSize = bufSize;

    int i;
    for (i = 1; i < bufSize - 1; i++)
        if ((*buf)[i] == FLAG || (*buf)[i] == ESCAPE)
            newBufSize++;

    *buf = (unsigned char *)realloc(*buf, newBufSize);

    for (i = 1; i < bufSize - 1; i++)
    {
        if ((*buf)[i] == FLAG || (*buf)[i] == ESCAPE)
        {
            memmove(*buf + i + 1, *buf + i, bufSize - i);

            bufSize++;

            (*buf)[i] = ESCAPE;
            (*buf)[i + 1] ^= 0x20;
        }
    }

    return newBufSize;
}

//Destuffing
int destuff(unsigned char **buf, int bufSize)
{
    int i;
    for (i = 1; i < bufSize - 1; ++i)
    {
        if ((*buf)[i] == ESCAPE)
        {
            memmove(*buf + i, *buf + i + 1, bufSize - i - 1);

            bufSize--;

            (*buf)[i] ^= 0x20;
        }
    }

    *buf = (unsigned char *)realloc(*buf, bufSize);

    return bufSize;
}

int llopen()
{

    printf("Estabilishing connection...\n");

    //=== Provided code ===

    int fd = open(settings->port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(settings->port);
        exit(ERROR);
    }

    if (tcgetattr(fd, &settings->oldtio) == ERROR)
    {
        perror("tcgetattr");
        exit(ERROR);
    }

    bzero(&settings->newtio, sizeof(settings->newtio));
    settings->newtio.c_cflag = settings->baudRate | CS8 | CLOCAL | CREAD;
    settings->newtio.c_iflag = IGNPAR;
    settings->newtio.c_oflag = 0;

    settings->newtio.c_lflag = 0;

    settings->newtio.c_cc[VTIME] = 1;
    settings->newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &settings->newtio) == ERROR)
    {
        perror("tcsetattr error");
        exit(ERROR);
    }

    //========================

    int tries = 0, connected = 0;

    //Writer mode
    if (settings->mode == WRITER)
    {
        while (!connected)
        {
            if (tries == 0 || alarmFired)
            {

                //Reset alarm
                alarmFired = FALSE;

                //Number of tries exceeded
                if (tries >= settings->numTries)
                {
                    stopAlarm();
                    printf("ERROR: Maximum number of retries exceeded.\n");
                    printf("CONNECTION ABORTED\n");
                    exit(ERROR);
                }

                //Send SET command
                sendCommand(fd, C_SET);

                //Restart alarm
                if (++tries == 1)
                    setAlarm();
            }

            //TODO Receive response
        }

        //Stop alarm
        stopAlarm();
    }
    //Reader mode
    else if (settings->mode == READER)
    {
    }

    printf("Connection established\n");

    return fd;
}