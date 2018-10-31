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
#include "receiver.h"

volatile int STOP = FALSE;
struct termios oldtio, newtio;
int seqNum = 0;
int lastSeq = 0;

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

void lastSeqsendControlMessage(int fd, int C)
{
    unsigned char message[UA_SIZE];

    message[0] = FLAG;
    message[1] = A;
    message[2] = C;
    message[3] = A ^ C;
    message[4] = FLAG;

    write(fd, message, UA_SIZE);
}

/* state machine */
int stateMachineSET(int fd, int controlChar)
{
    int state = START;
    char *buffer = (char *)malloc((SET_SIZE + 1) * sizeof(char));
    char c;
    int i = 0;

    while (state != STOP_STATE)
    {
        read(fd, &c, 1);

        switch (state)
        {
        case START:
            if (c == FLAG)
            {
                buffer[i++] = c;
                state = FLAG_RCV;
            }
            break;

        case FLAG_RCV:
            if (c == A)
            {
                state = A_RCV;
                buffer[i++] = c;
            }
            else if (c == FLAG)
                state = FLAG_RCV;
            else
            {
                memset(buffer, 0, SET_SIZE + 1);
                i = 0;
                state = START;
            }
            break;

        case A_RCV:
            if (c == controlChar)
            {
                state = C_RCV;
                buffer[i++] = c;
            }
            else if (c == FLAG)
                state = FLAG_RCV;
            else
            {
                memset(buffer, 0, SET_SIZE + 1);
                i = 0;
                state = START;
            }
            break;

        case C_RCV:
            if (c == (A ^ controlChar))
            {
                state = BCC_OK;
                buffer[i++] = c;
            }
            else
            {
                memset(buffer, 0, SET_SIZE + 1);
                i = 0;
                state = START;
            }
            break;

        case BCC_OK:
            if (c == FLAG)
            {
                printf("Final flag received. Message received completely\n");
                state = STOP_STATE;
                buffer[i++] = c;
            }
            else
            {
                memset(buffer, 0, SET_SIZE + 1);
                i = 0;
                state = START;
            }
            break;
        }
    }

    return TRUE;
}

int checkBCC2(char *message, int messageSize)
{
    int i;
    char BCC2 = message[4]; /* start from the information, after BCC1 */

    for (i = 5; i < messageSize - 2; i++)
    {
        BCC2 ^= message[i];
    }
    return BCC2 == message[messageSize - 2];
}

int llread(int fd, char *buffer)
{
    int state = START;
    char c, controlChar;
    int rej = 0, countChars = 0;

    while (state != STOP_STATE)
    {
        read(fd, &c, 1);
        countChars += 1;

        switch (state)
        {
        case START:
            if (c == FLAG)
            {
                buffer[countChars - 1] = c;
                state = FLAG_RCV;
            }
            break;

        case FLAG_RCV:
            if (c == A)
            {
                state = A_RCV;
                buffer[countChars - 1] = c;
            }
            else if (c == FLAG)
                state = FLAG_RCV;
            else
            {
                memset(buffer, MAX_DATA_SIZE, 0);
                countChars = 0;
                state = START;
            }
            break;

        case A_RCV:
            if (c == C_0)
            {

                seqNum = 1;
                state = C_RCV;
                buffer[countChars - 1] = c;
                controlChar = c;
            }
            else if (c == C_1)
            {

                seqNum = 0;
                state = C_RCV;
                buffer[countChars - 1] = c;
                controlChar = c;
            }
            else if (c == FLAG)
            {
                controlChar = c;
                state = FLAG_RCV;
            }
            else
            {
                memset(buffer, MAX_DATA_SIZE, 0);
                countChars = 0;
                state = START;
            }
            break;

        case C_RCV:
            if (c == (A ^ controlChar))
            {
                state = BCC_OK;
                buffer[countChars - 1] = c;
            }
            else
            {
                memset(buffer, MAX_DATA_SIZE, 0);
                countChars = 0;
                state = START;
            }
            break;

        case ESCAPE_STATE:
            if (c == 0x5E)
            { // 0x7D 0x5E => 0x7E (FLAG)
                countChars--;
                buffer[countChars - 1] = FLAG;
                state = BCC_OK;
            }
            else if (c == 0x5D)
            { // 0x7D 0x5D => 0x7D (escape_character)
                countChars--;
                buffer[countChars - 1] = escape_character;
                state = BCC_OK;
            }
            else
            {
                perror("Invalid character after escape.");
                return ERR;
            }
            break;

        case BCC_OK:
            if (c == FLAG)
            {
                buffer[countChars - 1] = c;
                if (checkBCC2(buffer, countChars))
                {
                    if (seqNum == 1)
                    {
                        lastSeqsendControlMessage(fd, RR_1);
                        printf("Sent RR_1\n");
                    }
                    else
                    {
                        lastSeqsendControlMessage(fd, RR_0);
                        printf("Sent RR_0\n");
                    }

                    state = STOP_STATE;
                }
                else
                {
                    rej = 1;
                    if (seqNum == 0)
                    {
                        lastSeqsendControlMessage(fd, REJ_0);
                        printf("Sent REJ_0\n");
                    }

                    else
                    {
                        lastSeqsendControlMessage(fd, REJ_1);
                        printf("Sent REJ_1\n");
                    }
                    state = STOP_STATE;
                }
            }
            else if (c == escape_character)
            {
                state = ESCAPE_STATE;
            }
            else
            {
                buffer[countChars - 1] = c;
            }
            break;
        }
    }

    if (rej)
        return 0;

    return countChars;
}

int llopen(int fd)
{

    if (tcgetattr(fd, &oldtio) == ERR)
    {
        perror("tcgetattr");
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

    //Own code:

    if (stateMachineSET(fd, SET_C))
    {
        printf("Received SET\n");
        lastSeqsendControlMessage(fd, UA_C);
        printf("Sent UA\n");
    }

    printf("LLOPEN done!");
}

void llclose(int fd)
{
    int state;

    state = DISC_C;
    if (stateMachineSET(fd, state))
    {
        printf("Received DISC\n");
        lastSeqsendControlMessage(fd, state);
    }

    state = UA_C;
    if (stateMachineSET(fd, state))
        printf("Receiver terminated\n");

    tcsetattr(fd, TCSANOW, &oldtio);
}

int checkTrailer(char *first, int sizeF, char *last, int sizeL)
{

    if (sizeF != sizeL || last[0] != '3')
    {
        return FALSE;
    }
    else
    {
        int i;
        for (i = 1; i < sizeF - 2; i++)
        {
            if (first[i] != last[i])
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

int compareMessages(char *previous, int previousSize, char *new, int newSize)
{
    printf("\nPrevious size: %d\n", previousSize);
    print_hexa_zero(previous, previousSize);
    printf("\nNew size: %d\n", newSize);
    print_hexa_zero(new, newSize);
    if (previousSize != newSize)
    {
        return 1;
    }
    else
    {
        int i;
        for (i = 0; i < newSize; i++)
        {
            if (previous[i] != new[i])
            {
                return 1;
            }
        }
    }

    return 0;
}

char *removeHeader(char *message, char messageSize, int *newSize, int *infoSize)
{

    /*Remove Header, Frame and Trailer*/

    int L1, L2;

    char *new_message = (char *)malloc(messageSize);
    L2 = message[6];
    L1 = message[7];

    *infoSize = L1 + 256 * L2;
    printf("infoSize: %d \n", *infoSize);
    int i;
    for (i = 0; i < *infoSize; i++)
    {
        new_message[i] = message[i + 8];
    }
    *newSize = *infoSize;

    return new_message;
}

void fileName(char *message, char *name)
{
    memset(name, 0, 100);

    int L1 = message[2] - '0';
    int L2_index = L1 + 4;
    printf("L1: %i\n L2_index: %i\n", L1, L2_index);
    int L2 = message[L2_index];
    printf("L2: %i\n", L2);

    int i;
    for (i = 0; i < L2; i++)
    {
        name[i] = message[5 + L1 + i];
    }

    name[L2] = '\0';
}

off_t fileSize(char *message)
{

    int L1 = message[2] - '0';
    printf("L1: %d\n", L1);
    off_t dec = 0;
    char *new = (char *)malloc(100 * sizeof(char));

    strncat(new, message + 3, L1);
    new[L1] = '\0';
    int i = 0;
    for (i = 0; i < strlen(new); i++)
    {
        dec = dec * 10 + (new[i] - '0');
    }

    if (dec >= (ULONG_MAX - 2))
    {
        return 0;
    }
    return dec;
}

void receiveFile(int fd)
{

    int messageSize;
    FILE *file_out;
    int sizeF_message;
    int size_without_header;
    int infoSize = 0;
    int previousSize;

    int compared;
    off_t file_index = 0;
    char *first_message = (char *)malloc((MAX_DATA_SIZE) * sizeof(char));
    char *new_message = (char *)malloc((MAX_DATA_SIZE) * sizeof(char));
    char *message = (char *)malloc((MAX_DATA_SIZE) * sizeof(char));
    char *previous_message = (char *)malloc((MAX_DATA_SIZE) * sizeof(char));
    char *message_to_compare = (char *)malloc((MAX_DATA_SIZE) * sizeof(char));
    char *file_name = (char *)malloc(100 * sizeof(char));

    sizeF_message = llread(fd, first_message);

    int n = 4;
    int i;
    for (i = 0; i < sizeF_message; i++)
    {
        new_message[i] = first_message[n];
        n++;
    }
    sizeF_message -= 4;

    fileName(new_message, file_name);

    printf("File name: %s\n", file_name);

    off_t file_size = fileSize(new_message);

    if (file_size == 0)
    {
        printf("File not found on the writer side.\n");
        exit(ERR);
    }

    printf("File Size: %lu\n", file_size);

    file_out = fopen(file_name, "wb+");
    int packet_number = 1;
    while (TRUE)
    {

        printf("***Packet number %d***\n", packet_number);
        printf("seqNum: %d\n", seqNum);

        memset(message, 0, FRAGMENT_SIZE);
        messageSize = llread(fd, message);
        printf("Message size: %d\n", messageSize);

        if (packet_number > 1)
        {
            compared = compareMessages(previous_message, previousSize, message, messageSize);
        }

        if (messageSize == 0 || messageSize == ERR || compared == 0)
        {
            if (compared = 0)
            {
                printf("Same messages!!!\n");
            }
            printf("REJECTED PACKAGE\n");
            continue;
        }

        memset(previous_message, 0, FRAGMENT_SIZE);
        memcpy(previous_message, message, messageSize);
        previousSize = messageSize;

        int n = 4;
        int i = 0;
        for (i = 0; i < messageSize; i++)
        {
            message_to_compare[i] = message[n];
            n++;
        }
        int message_to_compare_size = messageSize - 4;

        if (checkTrailer(new_message, sizeF_message, message_to_compare, message_to_compare_size))
        {
            printf("Trailer Message!\n");
            break;
        }

        message = removeHeader(message, messageSize, &size_without_header, &infoSize);

        fwrite(message, 1, infoSize, file_out);
        packet_number++;
    }

    fclose(file_out);
}

int main(int argc, char **argv)
{
    int fd, c, res;
    char buf[255];

    if ((argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
         (strcmp("/dev/ttyS1", argv[1]) != 0)))
    {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(argv[1]);
        exit(ERR);
    }

    llopen(fd);
    receiveFile(fd);
    llclose(fd);

    close(fd);
    return 0;
}
