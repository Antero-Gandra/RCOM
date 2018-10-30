#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "writer.h"
#include <errno.h>
#include <math.h>

volatile int STOP = FALSE;
struct termios oldtio, newtio;
int alarmActive, alarmCount, received, endUA, contor;
int seqNum = 0, packetNum = 0;
char receivedUA[UA_SIZE + 1];

void printHex(char *str)
{
    int j;
    for (j = 0; j < strlen(str); j++)
    {
        printf("%0xh ", str[j]);
    }
    printf("\n");
}

void printHexZero(char *str, int len)
{
    int j;
    for (j = 0; j < len; j++)
    {
        printf("%0xh ", str[j]);
    }
    printf("\n");
}

/* listens to the alarm */
void alarmHandler()
{
    printf("%d sec timeout - Alarm count: %d\n", TIMEOUT, alarmCount);
    alarmActive = TRUE;
    alarmCount++;
}

void disableAlarm()
{
    alarmActive = 0;
    alarmCount = 0;
    alarm(0);
}

/* builds and sends to fd tge SET control message */
void sendControlMessage(int fd, int C)
{
    unsigned char messageSET[SET_SIZE];

    messageSET[0] = FLAG;
    messageSET[1] = A;
    messageSET[2] = C;
    messageSET[3] = A ^ C;
    messageSET[4] = FLAG;

    printf("Transmit: ");
    printHex(messageSET);
    write(fd, messageSET, SET_SIZE);
}

void stateMachineUA(int *state, unsigned char *c)
{
    switch (*state)
    {
    case START:
        if (*c == FLAG)
        {
            *state = FLAG_RCV;
            receivedUA[contor++] = *c;
        }
        break;
    case FLAG_RCV:
        if (*c == A)
        {
            *state = A_RCV;
            receivedUA[contor++] = *c;
        }
        else
        {
            if (*c == FLAG)
                *state = FLAG_RCV;
            else
            {
                memset(receivedUA, 0, UA_SIZE + 1);
                *state = START;
            }
        }
        break;
    case A_RCV:
        if (*c == UA_C)
        {
            *state = C_RCV;
            receivedUA[contor++] = *c;
        }
        else
        {
            if (*c == FLAG)
                *state = FLAG_RCV;
            else
            {
                memset(receivedUA, 0, UA_SIZE + 1);
                *state = START;
            }
        }
        break;

    case C_RCV:
        if (*c == UA_C ^ A)
        {
            *state = BCC_OK;
            receivedUA[contor++] = *c;
        }
        else
        {
            memset(receivedUA, 0, UA_SIZE + 1);
            *state = START;
        }
        break;

    case BCC_OK:
        if (*c == FLAG)
        {
            receivedUA[contor++] = *c;
            endUA = TRUE;
            disableAlarm();
            printf("State Machine UA OK!\n");
        }
        else
        {
            memset(receivedUA, 0, UA_SIZE + 1);
            *state = START;
        }
        break;
    }
}
int llopen(int fd)
{

    char *buf = (char *)malloc((UA_SIZE + 1) * sizeof(char));
    int i = 0, state;
    char c;

    if (tcgetattr(fd, &oldtio) == ERR)
    {
        perror("tcgetattr error");
        exit(ERR);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == ERR)
    {
        perror("tcsetattr error");
        exit(ERR);
    }

    printf("New termios structure set\n");

    /* Sends the SET control message, waits for UA for TIMEOUT seconds.
    If it does not receive UA, it resends SET, this operation being
    repeated for MAX_ALARMS times */

    alarmActive = TRUE;
    while (alarmCount < MAX_ALARMS && alarmActive)
    {
        sendControlMessage(fd, SET_C); /* send SET */
        alarm(TIMEOUT);
        alarmActive = FALSE;
        state = START;

        while (!endUA && !alarmActive)
        {
            read(fd, &c, 1);
            //printf("We read %c\n", c);
            stateMachineUA(&state, &c);
        }
    }

    if (alarmCount == MAX_ALARMS && alarmActive)
    {
        printf("LLOPEN returning false.\n");
        return FALSE;
    }
    else
    {
        disableAlarm();
        printf("LLOPEN OK!\n");
        return TRUE;
    }
}

/* reads control message and returns the control character in the
control message; also, uses the state machine to check if the message
is correctly received */
int readControlMessage(int fd)
{
    int step = START;
    unsigned char c;
    int returnValue = ERR;

    while (!alarmActive && step != STOP_STATE)
    {
        read(fd, &c, 1);
        switch (step)
        {
        // FLAG
        case START:
            if (c == FLAG)
                step = FLAG_RCV;
            break;
        // A
        case FLAG_RCV:
            if (c == A)
            {
                step = A_RCV;
            }
            else if (c == FLAG)
            {
                step = FLAG_RCV;
            }
            else
            {
                step = START;
            }
            break;
        // C
        case A_RCV:
            if (c == RR_0 || c == RR_1 || c == REJ_0 || c == REJ_1 || c == DISC)
            {
                returnValue = c;
                step = C_RCV;
            }
            else if (c == FLAG)
            {
                step = FLAG_RCV;
            }
            else
            {
                step = START;
            }
            break;
        // BCC2
        case C_RCV:
            if (c == (A ^ returnValue))
                step = BCC_OK;
            else
                step = START;
            break;

        case BCC_OK:
            if (c == FLAG)
            {
                alarm(0);
                step = STOP_STATE;
                return returnValue;
            }
            else
                step = START;
            break;
        }
    }
    return returnValue;
}

/* closes the connection between sender and receiver */
void llclose(int fd)
{
    char c;
    int control_char, state;

    control_char = DISC;
    sendControlMessage(fd, control_char);

    control_char = 0;

    read(fd, &c, 1);
    state = START;
    while (state != STOP)
    {

        //control_char = readControlMessage(fd);
        stateMachineUA(&state, &c);
        printf("State is: %d\n", state);
    }

    printf("[llclose]State after while is: %d\n", state);

    control_char = UA_C;

    sendControlMessage(fd, control_char);

    tcsetattr(fd, TCSANOW, &oldtio);
    printf("Sender terminated!\n.");
}

/* gets the file size in bytes of a given file */
off_t fsize(const char *filename)
{
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    fprintf(stderr, "Cannot determine size of %s: %s\n",
            filename, strerror(errno));

    return ERR;
}

//Create a control packet with the structure C T1 L1 V1 T2 L2 V2
int createControlPacket(char *controlPacket, int packet_type, int file_size, char *fileName)
{
    char buffer[BUF_SIZE];
    int bytes_written = 5;
    int L1;
    int L2;
    int pos = 0;

    memset(buffer, 0, BUF_SIZE);
    sprintf(buffer, "%d", packet_type);
    strcat(controlPacket, buffer);

    memset(buffer, 0, BUF_SIZE);
    sprintf(buffer, "%d", FILE_SIZE_CODE);
    strcat(controlPacket, buffer);

    memset(buffer, 0, BUF_SIZE);
    L1 = floor(log10(abs(file_size))) + 1;
    sprintf(buffer, "%d", L1);
    strcat(controlPacket, buffer);

    memset(buffer, 0, BUF_SIZE);
    sprintf(buffer, "%d", file_size);
    strcat(controlPacket, buffer);
    bytes_written += L1;

    memset(buffer, 0, BUF_SIZE);
    sprintf(buffer, "%d", FILE_NAME_CODE);
    strcat(controlPacket, buffer);

    memset(buffer, 0, BUF_SIZE);
    L2 = strlen(fileName);
    controlPacket[L1 + 4] = L2;

    strcat(controlPacket, fileName);
    bytes_written += L2;

    return bytes_written;
}

/* 
    Create I frame with data
    returns the number of bytes in the frame 
*/
int frameData(char *message, char *buffer, int length)
{

    int message_size, i;

    //Calculate BCC2
    char BCC2;
    char BCC2_stuffed[2];
    int double_BCC2 = FALSE;

    BCC2 = buffer[0];
    for (i = 1; i < length; i++)
        BCC2 ^= buffer[i];

    if (BCC2 == FLAG)
    {
        BCC2_stuffed[0] = ESC;
        BCC2_stuffed[1] = ESC_AFTER;
        double_BCC2 = TRUE;
    }
    else if (BCC2 == ESC)
    {
        BCC2_stuffed[0] = ESC;
        BCC2_stuffed[1] = ESC_ESC;
        double_BCC2 = TRUE;
    }

    //Generate message
    message[0] = FLAG;
    message[1] = A;

    if (seqNum == 0)
    {
        message[2] = C_0;
    }
    else
    {
        message[2] = C_1;
    }

    message[3] = (message[1] ^ message[2]);

    int n = 4;
    message_size = length + FRAME_SIZE;
    for (i = 0; i < length; i++)
    {
        if (buffer[i] == FLAG)
        {
            message_size += 1;
            message = (char *)realloc(message, message_size);
            message[n] = ESC;
            message[n + 1] = ESC_AFTER;
            n += 2;
        }
        else if (buffer[i] == ESC)
        {
            message_size += 1;
            message = (char *)realloc(message, message_size);
            message[n] = ESC;
            message[n + 1] = ESC_ESC;
            n += 2;
        }
        else
        {
            message[n] = buffer[i];
            n += 1;
        }
    }

    if (!double_BCC2)
    {
        message[n] = BCC2;
        n += 1;
    }
    else
    {
        message_size += 1;
        message = (char *)realloc(message, message_size);
        message[n] = BCC2_stuffed[0];
        message[n + 1] = BCC2_stuffed[1];
        n += 2;
    }
    message[n] = FLAG;

    return n + 1;
}

int llwrite(int fd, char *message, int frameSize)
{

    int i, rejected = FALSE, rejectedCount = 0, tryCount = 0;

    do
    {
        tryCount++;

        printf("Try number: %d\n", tryCount);

        write(fd, message, frameSize);

        alarmActive = FALSE;
        alarm(TIMEOUT);

        int C = readControlMessage(fd);

        if ((C == RR_1 && seqNum == 0) || (C == RR_0 && seqNum == 1))
        {
            if (C == RR_0)
                printf("Received RR_0\n");
            if (C == RR_1)
                printf("Received RR_1\n");

            rejected = FALSE;
            alarmCount = 0;

            if (seqNum == 0)
            {
                seqNum = 1;
            }
            else
            {
                seqNum = 0;
            }
            alarm(0);
        }
        else if (C == REJ_0 || C == REJ_1 || C == ERR)
        {
            printf("Sequence Number = %d \n", seqNum);

            rejected = TRUE;
            rejectedCount++;

            if (C == REJ_0)
                printf("RECEIVED REJ_0\n");
            if (C == REJ_1)
                printf("RECEIVED REJ_1\n");
            if (C == ERR)
                printf("RECEIVED ERR\n");
            printf("Rejected Count: %d\n", rejectedCount);
            alarm(0);
        }

    } while ((alarmActive && (alarmCount < MAX_ALARMS)) ||
             (rejected && (rejectedCount < MAX_REJECTIONS)));

    if (alarmCount >= MAX_ALARMS)
        return ERR;

    printf("Number of bytes written: %d \n", frameSize);
    return frameSize;
}

void createDataPacket(char *dataPacket, char *buffer, int length, int seq)
{
    dataPacket[0] = DATA_PACKET;
    dataPacket[1] = seq % 255;
    dataPacket[2] = length / 256;                 // L2
    dataPacket[3] = length - 256 * dataPacket[2]; // L1
    memcpy(dataPacket + 4, buffer, length);
}

int send_file(int fd, char *fileName)
{

    FILE *fp;
    char *controlPacket = (char *)malloc(CONTROL_MESSAGE_LEN * sizeof(char));
    char *dataPacket = (char *)malloc((FRAGMENT_SIZE + 4) * sizeof(char));
    int controlPacketSize, dataPacketSize, bytesRead, end = FALSE, seq = 0;
    char buffer[FRAGMENT_SIZE];
    int bytesPosFraming;
    int frameSize = FRAGMENT_SIZE + PH_SIZE + FRAME_SIZE;
    char *message = (char *)malloc(frameSize * sizeof(char));

    // Control Packet
    printf("File to send: size of %s is %li bytes\n", fileName, fsize(fileName));
    controlPacketSize = createControlPacket(controlPacket, START_PACKET, fsize(fileName), fileName);

    printf("Send start control packet: %s of size = %d\n", controlPacket, controlPacketSize);

    /* put controlPacket into I frames */
    memset(message, 0, frameSize);
    bytesPosFraming = frameData(message, controlPacket, controlPacketSize);

    llwrite(fd, message, bytesPosFraming);
    packetNum++;
    seq++;

    /* open file to transmit in read mode */
    fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        printf("File does not exist\n");
        exit(-1);
    }

    while (!end)
    {
        memset(buffer, 0, FRAGMENT_SIZE);
        bytesRead = fread(buffer, 1, FRAGMENT_SIZE, fp);

        if (bytesRead < FRAGMENT_SIZE)
        {
            //printf("End of file detected\n");
            end = TRUE;
        }

        /* create dataPacket */
        memset(dataPacket, 0, FRAGMENT_SIZE + PH_SIZE);
        createDataPacket(dataPacket, buffer, bytesRead, seq);
        dataPacketSize = bytesRead + PH_SIZE;

        /* frame the dataPacket into a I frame */
        memset(message, 0, frameSize);
        bytesPosFraming = frameData(message, dataPacket, dataPacketSize);

        int r = llwrite(fd, message, bytesPosFraming);
        packetNum++;
        seq++;

        if (r == ERR)
        {
            printf("Max number of alarms reached\n");
            return ERR;
        }
    }

    /* create END controlPacket */
    memset(controlPacket, 0, controlPacketSize);
    createControlPacket(controlPacket, END_PACKET, fsize(fileName), fileName);

    /* encapsulate into I frame */
    memset(message, 0, frameSize);
    bytesPosFraming = frameData(message, controlPacket, controlPacketSize);
    printf("Send STOP control packet: %s of size = %d\n", controlPacket, controlPacketSize);
    llwrite(fd, message, bytesPosFraming);
    packetNum++;

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    int fd, c, res;
    char buf[255], generatedBuffer[7];
    int i, sum = 0, speed = 0;

    if ((argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0)))
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(argv[1]);
        exit(ERR);
    }

    signal(SIGALRM, alarmHandler); /* link SIGALRM with alarmHandler function */

    llopen(fd);
    send_file(fd, "pinguim.gif");
    llclose(fd);

    close(fd);
    return 0;
}
