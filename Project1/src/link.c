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

const int DEBUG_MODE = 1;

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
    unsigned char *command = malloc(commandMaxSize);

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

int messageIsCommand(Message *msg, Command command)
{
    return msg->type == COMMAND && msg->command == command;
}

Message *receiveMessage(int fd)
{
    Message *msg = (Message *)malloc(sizeof(Message));
    msg->type = INVALID;
    msg->ns = msg->nr = -1;

    State state = START;

    int size = 0;
    unsigned char *message = malloc(settings->messageDataMaxSize);

    volatile int done = FALSE;
    while (!done)
    {
        unsigned char c;

        //Not stopping yet
        if (state != STOP)
        {
            //Read
            int numReadBytes = read(fd, &c, 1);

            //Empty
            if (!numReadBytes)
            {
                if (DEBUG_MODE)
                    printf("ERROR: nothing received.\n");

                free(message);

                msg->type = INVALID;
                msg->error = INPUT_OUTPUT_ERROR;

                return msg;
            }
        }

        //State Machine
        switch (state)
        {
        case START:
            if (c == FLAG)
            {
                if (DEBUG_MODE)
                    printf("START: FLAG received. Going to FLAG_RCV.\n");

                message[size++] = c;

                state = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if (c == A)
            {
                if (DEBUG_MODE)
                    printf("FLAG_RCV: A received. Going to A_RCV.\n");

                message[size++] = c;

                state = A_RCV;
            }
            else if (c != FLAG)
            {
                size = 0;

                state = START;
            }
            break;
        case A_RCV:
            if (c != FLAG)
            {
                if (DEBUG_MODE)
                    printf("A_RCV: C received. Going to C_RCV.\n");

                message[size++] = c;

                state = C_RCV;
            }
            else if (c == FLAG)
            {
                size = 1;

                state = FLAG_RCV;
            }
            else
            {
                size = 0;

                state = START;
            }
            break;
        case C_RCV:
            if (c == (message[1] ^ message[2]))
            {
                if (DEBUG_MODE)
                    printf("C_RCV: BCC received. Going to BCC_OK.\n");

                message[size++] = c;

                state = BCC_OK;
            }
            else if (c == FLAG)
            {
                if (DEBUG_MODE)
                    printf("C_RCV: FLAG received. Going back to FLAG_RCV.\n");

                size = 1;

                state = FLAG_RCV;
            }
            else
            {
                if (DEBUG_MODE)
                    printf("C_RCV: ? received. Going back to START.\n");

                size = 0;

                state = START;
            }
            break;
        case BCC_OK:
            if (c == FLAG)
            {
                if (msg->type == INVALID)
                    msg->type = COMMAND;

                message[size++] = c;

                state = STOP;

                if (DEBUG_MODE)
                    printf("BCC_OK: FLAG received. Going to STOP.\n");
            }
            else if (c != FLAG)
            {
                if (msg->type == INVALID)
                    msg->type = DATA;
                else if (msg->type == COMMAND)
                {
                    printf("WARNING?? something unexpected happened.\n");
                    state = START;
                    continue;
                }

                // if writing at the end and more bytes will still be received
                if (size % settings->messageDataMaxSize == 0)
                {
                    int mFactor = size / settings->messageDataMaxSize + 1;
                    message = (unsigned char *)realloc(message,
                                                       mFactor * settings->messageDataMaxSize);
                }

                message[size++] = c;
            }
            break;
        case STOP:
            message[size] = 0;
            done = TRUE;
            break;
        default:
            break;
        }
    }

    size = destuff(&message, size);

    unsigned char A = message[1];
    unsigned char C = message[2];
    unsigned char BCC1 = message[3];

    if (BCC1 != (A ^ C))
    {
        printf("ERROR: invalid BCC1.\n");

        free(message);

        msg->type = INVALID;
        msg->error = BCC1_ERROR;

        return msg;
    }

    if (msg->type == COMMAND)
    {
        // get message command

        switch (message[2] & 0x0F)
        {
        case C_SET:
            msg->command = SET;
        case C_UA:
            msg->command = UA;
        case C_RR:
            msg->command = RR;
        case C_REJ:
            msg->command = REJ;
        case C_DISC:
            msg->command = DISC;
        default:
            printf("ERROR: control field not recognized.\n");
            msg->command = SET;
        }

        // get command control field
        Control controlField = message[2];

        if (msg->command == RR || msg->command == REJ)
            msg->nr = (controlField >> 7) & BIT(0);
    }
    else if (msg->type == DATA)
    {
        msg->data.messageSize = size - 6 * sizeof(char);

        unsigned char calcBCC2 = processBCC(&message[4], msg->data.messageSize);
        unsigned char BCC2 = message[4 + msg->data.messageSize];

        if (calcBCC2 != BCC2)
        {
            printf("ERROR: invalid BCC2: 0x%02x != 0x%02x.\n", calcBCC2, BCC2);

            free(message);

            msg->type = INVALID;
            msg->error = BCC2_ERROR;

            return msg;
        }

        msg->ns = (message[2] >> 6) & BIT(0);

        // copy message
        msg->data.message = malloc(msg->data.messageSize);
        memcpy(msg->data.message, &message[4], msg->data.messageSize);
    }

    free(message);

    return msg;
}

unsigned char processBCC(const unsigned char* buf, int size) {
	unsigned char BCC = 0;

	int i = 0;
	for (; i < size; i++)
		BCC ^= buf[i];

	return BCC;
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

            //Receive response
            if (messageIsCommand(receiveMessage(fd), UA))
                connected = 1;
        }

        //Stop alarm
        stopAlarm();
    }
    //Reader mode
    else if (settings->mode == READER)
    {
        while (!connected)
        {
            //Receive setup and respond
            if (messageIsCommand(receiveMessage(fd), SET))
            {
                sendCommand(fd, UA);
                connected = 1;
            }
        }
    }

    printf("Connection established\n");

    return fd;
}