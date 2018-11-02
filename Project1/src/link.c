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
    settings->ns = 0;
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
        printf("ERROR: Could not write %s command.\n", command);

    //Free command
    free(command);
}

int identifyMessageControl(Message *msg, Control command)
{
    return msg->type == COMMAND && msg->control == command;
}

Message *receiveMessage(int fd)
{
    //Setup message
    Message *msg = (Message *)malloc(sizeof(Message));
    msg->type = INVALID;
    msg->ns = msg->nr = -1;

    State state = START;

    int size = 0;
    unsigned char *message = malloc(settings->messageDataMaxSize);

    //State Machine
    volatile int done = FALSE;
    while (!done)
    {
        unsigned char ch;

        //Not stopping yet
        if (state != STOP)
        {
            //Read
            int numReadBytes = read(fd, &ch, 1);

            //Empty
            if (!numReadBytes)
            {
                free(message);

                msg->type = INVALID;
                msg->error = INPUT_OUTPUT_ERROR;

                return msg;
            }
        }

        //State jumping
        switch (state)
        {
        case START:
            if (ch == FLAG)
            {
                message[size++] = ch;

                state = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if (ch == A)
            {
                message[size++] = ch;

                state = A_RCV;
            }
            else if (ch != FLAG)
            {
                size = 0;

                state = START;
            }
            break;
        case A_RCV:
            if (ch != FLAG)
            {
                message[size++] = ch;

                state = C_RCV;
            }
            else if (ch == FLAG)
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
            if (ch == (message[1] ^ message[2]))
            {
                message[size++] = ch;

                state = BCC_OK;
            }
            else if (ch == FLAG)
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
        case BCC_OK:
            if (ch == FLAG)
            {
                if (msg->type == INVALID)
                    msg->type = COMMAND;

                message[size++] = ch;

                state = STOP;
            }
            else if (ch != FLAG)
            {
                if (msg->type == INVALID)
                    msg->type = DATA;

                //Need to expand space
                if (size % settings->messageDataMaxSize == 0)
                {
                    int mFactor = size / settings->messageDataMaxSize + 1;
                    message = (unsigned char *)realloc(message, mFactor * settings->messageDataMaxSize);
                }

                message[size++] = ch;
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

    //Destuff
    size = destuff(&message, size);

    unsigned char A = message[1];
    unsigned char C = message[2];
    unsigned char BCC1 = message[3];

    //BCC1 check (header)
    if (BCC1 != (A ^ C))
    {
        printf("ERROR: invalid BCC1.\n");

        free(message);

        msg->type = INVALID;
        msg->error = BCC1_ERROR;

        return msg;
    }

    //Message is a command
    if (msg->type == COMMAND)
    {

        //Identify command
        switch (message[2] & 0x0F)
        {
        case C_SET:
            msg->control = C_SET;
            break;
        case C_UA:
            msg->control = C_UA;
            break;
        case C_RR:
            msg->control = C_RR;
            break;
        case C_REJ:
            msg->control = C_REJ;
            break;
        case C_DISC:
            msg->control = C_DISC;
            break;
        default:
            printf("ERROR: control field not recognized.\n");
            msg->control = C_SET;
        }

        //Control
        Control control = message[2];

        if (msg->control == C_RR || msg->control == C_REJ)
            msg->nr = (control >> 7) & BIT(0);
    }
    //Message is data
    else if (msg->type == DATA)
    {
        msg->data.size = size - 6 * sizeof(char);

        //Check BCC2 (data)
        unsigned char calcBCC2 = processBCC(&message[4], msg->data.size);
        unsigned char BCC2 = message[4 + msg->data.size];

        if (calcBCC2 != BCC2)
        {
            printf("ERROR: invalid BCC2: 0x%02x != 0x%02x.\n", calcBCC2, BCC2);

            free(message);

            msg->type = INVALID;
            msg->error = BCC2_ERROR;

            return msg;
        }

        msg->ns = (message[2] >> 6) & BIT(0);

        //Copy the message
        msg->data.message = malloc(msg->data.size);
        memcpy(msg->data.message, &message[4], msg->data.size);
    }

    free(message);

    return msg;
}

void sendMessage(int fd, const unsigned char *message, int messageSize)
{

    //Setup message
    unsigned char *msg = malloc(6 * sizeof(char) + messageSize);

    unsigned char C = settings->ns << 6;
    unsigned char BCC1 = A ^ C;
    unsigned char BCC2 = processBCC(message, messageSize);

    msg[0] = FLAG;
    msg[1] = A;
    msg[2] = C;
    msg[3] = BCC1;

    memcpy(&msg[4], message, messageSize);

    msg[4 + messageSize] = BCC2;
    msg[5 + messageSize] = FLAG;

    messageSize += 6 * sizeof(char);

    //Stuffing
    messageSize = stuff(&msg, messageSize);

    //Send
    int numWrittenBytes = write(fd, msg, messageSize);
    if (numWrittenBytes != messageSize)
        perror("ERROR: error while sending message.\n");

    //free
    free(msg);
}

unsigned char processBCC(const unsigned char *buf, int size)
{
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
            if (identifyMessageControl(receiveMessage(fd), C_UA))
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
            if (identifyMessageControl(receiveMessage(fd), C_SET))
            {
                sendCommand(fd, C_UA);
                connected = 1;
            }
        }
    }

    printf("Connection established\n");

    return fd;
}

int llclose(int fd) {
	printf("*** Terminating connection. ***\n");

	int try = 0, disconnected = 0;

	switch (settings->mode) {
	case WRITER: {
		while (!disconnected) {
			if (try == 0 || alarmFired) {
				alarmFired = FALSE;

				if (try >= settings->numTries) {
					stopAlarm();
					printf("ERROR: Maximum number of retries exceeded.\n");
					printf("*** Connection aborted. ***\n");
					return 0;
				}

				sendCommand(fd, C_DISC);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receiveMessage(fd), C_DISC))
				disconnected = TRUE;
		}

		stopAlarm();
		sendCommand(fd, C_UA);
		sleep(1);

		printf("*** Connection terminated. ***\n");

		return 1;
	}
	case READER: {
		while (!disconnected) {
			if (messageIsCommand(receiveMessage(fd), C_DISC))
				disconnected = TRUE;
		}

		int uaReceived = FALSE;
		while (!uaReceived) {
			if (try == 0 || alarmFired) {
				alarmFired = FALSE;

				if (try >= settings->numTries) {
					stopAlarm();
					printf("ERROR: Disconnect could not be sent.\n");
					return 0;
				}

				sendCommand(fd, C_DISC);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receiveMessage(fd), C_UA))
				uaReceived = TRUE;
		}

		stopAlarm();
		printf("*** Connection terminated. ***\n");

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int llwrite(int fd, const unsigned char *buf, int bufSize)
{

    int tries = 0;

    while (1)
    {
        if (tries == 0 || alarmFired)
        {
            alarmFired = 0;

            if (tries >= settings->numTries)
            {
                stopAlarm();
                printf("ERROR: Maximum number of retries exceeded.\n");
                return 0;
            }

            //Send message
            sendMessage(fd, buf, bufSize);

            if (++tries == 1)
                setAlarm();
        }

        //Response
        Message *receivedMessage = receiveMessage(fd);

        //Receiver ready / positive ACK
        if (identifyMessageControl(receivedMessage, C_RR))
        {
            if (settings->ns != receivedMessage->nr)
                settings->ns = receivedMessage->nr;

            stopAlarm();
            break;
        }
        //Reject / negative ACK
        else if (identifyMessageControl(receivedMessage, C_REJ))
        {
            stopAlarm();
            tries = 0;
        }
    }

    stopAlarm();

    return 1;
}

int llread(int fd, unsigned char **message)
{
    Message *msg = NULL;

    int done = FALSE;
    while (!done)
    {
        //Read message
        msg = receiveMessage(fd);

        //Message type
        switch (msg->type)
        {
        case INVALID:
            //BCC error
            if (msg->error == BCC2_ERROR)
            {
                settings->ns = msg->ns;
                sendCommand(fd, C_REJ);
            }
            break;
        case COMMAND:
            //DISC command
            if (msg->control == C_DISC)
                done = TRUE;
            break;
        case DATA:
            //Check message order
            if (settings->ns == msg->ns)
            {
                *message = malloc(msg->data.size);
                memcpy(*message, msg->data.message, msg->data.size);
                free(msg->data.message);

                //Send response
                settings->ns = !msg->ns;
                sendCommand(fd, C_RR);

                done = TRUE;
            }
            else
                printf("Wrong message ns associated: ignoring\n");

            break;
        default:
            stopAlarm();
            return -1;
        }
    }

    //Alarm stopped by receiver
    stopAlarm();

    return 1;
}