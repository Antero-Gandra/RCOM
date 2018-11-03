
#pragma once

#include <termios.h>
#include <time.h> 

#define ERROR -1

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

typedef enum
{
    WRITER,
    READER
} Mode;

typedef enum
{
    C_SET = 0x03,
    C_UA = 0x07,
    C_RR = 0x05,
    C_REJ = 0x01,
    C_DISC = 0x0B
} Control;

typedef enum
{
    COMMAND,
    DATA,
    INVALID
} MessageType;

typedef enum
{
    INPUT_OUTPUT_ERROR,
    BCC1_ERROR,
    BCC2_ERROR
} ErrorType;

struct Data
{
    unsigned char *message;
    int size;
};

typedef struct
{
    MessageType type;
    ErrorType error;
    Control control;

    int ns;
    int nr;

    struct Data data;

} Message;

typedef struct
{
    struct timespec startTime; 

    int sent;
    int received;

    int timeouts;

    int sentRR;
    int sentREJ;
    int receivedRR;
    int receivedREJ;

} Stats;

#define BIT(n) (0x01 << n)

typedef struct
{

    char port[20];
    Mode mode;
    int baudRate;
    int messageDataMaxSize;
    int ns;
    int timeout;
    int numTries;
    char frame[256];
    struct termios oldtio, newtio;
    int errorChance;

} Settings;

extern Settings *settings;
extern Stats *stats;

void connectionSettings(char *port, Mode mode);
void statisticsSetup();
void printStats();

void sendCommand(int fd, Control com);
void sendMessage(int fd, const unsigned char *message, int messageSize);

int identifyMessageControl(Message *msg, Control command);
Message *receiveMessage(int fd);
unsigned char processBCC(const unsigned char *buf, int size);

int stuff(unsigned char **buf, int bufSize);
int destuff(unsigned char **buf, int bufSize);

int llopen();
int llclose(int fd);
int llwrite(int fd, const unsigned char *buf, int bufSize);
int llread(int fd, unsigned char **message);
