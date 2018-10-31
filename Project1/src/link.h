
#pragma once

#include <termios.h>

#define ERROR -1

typedef enum {
	WRITER, READER
} Mode;

typedef enum {
	SET, UA, RR, REJ, DISC
} Command;

typedef enum {
	C_SET = 0x03, C_UA = 0x07, C_RR = 0x05, C_REJ = 0x01, C_DISC = 0x0B
} Control;

typedef struct {

	char port[20];
	Mode mode;
	int baudRate;
	int messageDataMaxSize;
	int ns;
	int timeout;
	int numTries;
	char frame[256];
	struct termios oldtio, newtio;

} Settings;

extern Settings* settings;

void connectionSettings(char* port, Mode mode);

void sendCommand(int fd, Control com);

int stuff(unsigned char** buf, int bufSize);
int destuff(unsigned char** buf, int bufSize);

int llopen();
