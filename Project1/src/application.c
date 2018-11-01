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

    //File to be sent (and Mode assignment)
    Mode mode;
    if (argc == 3)
    {
        mode = WRITER;
        printf("File provided to be sent: %s\n", argv[2]);
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

    //Open connection
    fd = llopen();

    //TODO Send/Receive file
    if (settings->mode == WRITER)
        sendFile(argv[2], fd);
    //else
    //receiveFile();

    //TODO Close connection


    return 0;
}

FILE *openFile(char *fileName)
{

    //Open file
    FILE *file = fopen(fileName, "rb");

    if (!file)
    {
        printf("ERROR: Could not open file, exiting...\n");
        exit(ERROR);
    }

    return file;
}

int fileSize(FILE *file)
{

    //Save start position
    long int currentPosition = ftell(file);

    //End of file
    if (fseek(file, 0, SEEK_END) == -1)
    {
        printf("ERROR: Could not get file size.\n");
        exit(ERROR);
    }

    //Size
    long int size = ftell(file);

    //Recover start position
    fseek(file, 0, currentPosition);

    return size;
}

void sendControl(int fd, int cmd, char *fileS, char *fileName)
{

    //Package size
    int packageS = strlen(fileS) + strlen(fileName) + 5;

    //Setup package
    unsigned char controlPackage[packageS];

    int index = 0;
    int i;

    //Command
    controlPackage[index++] = cmd;

    //File size
    controlPackage[index++] = FILE_SIZE;
    controlPackage[index++] = strlen(fileS);
    for (i = 0; i < strlen(fileS); i++)
        controlPackage[index++] = fileS[i];

    //File name
    controlPackage[index++] = FILE_NAME;
    controlPackage[index++] = strlen(fileName);
    for (i = 0; i < strlen(fileName); i++)
        controlPackage[index++] = fileName[i];

    //Print information
    if (cmd == CTRL_START)
    {
        printf("File name: %s\n", fileName);
        printf("File size: %s (bytes)\n", fileS);
    }

    //Send package
    if (!llwrite(fd, controlPackage, packageS))
    {
        printf("ERROR: Could not send control package to link\n");
        free(controlPackage);
        exit(ERROR);
    }
}

void sendData(int fd, int N, const char* buffer, int length) {

    //Construct header
	unsigned char C = CTRL_DATA;
	unsigned char L2 = length / 256;
	unsigned char L1 = length % 256;

	//Package size
	int packageSize = 4 + length;

	//Allocate all space
	unsigned char* package = (unsigned char*) malloc(packageSize);

	//Package Header
	package[0] = C;
	package[1] = N;
	package[2] = L2;
	package[3] = L1;

	//Copy chunk to package
	memcpy(&package[4], buffer, length);

	//Write package
	if (!llwrite(fd, package, packageSize)) {
		printf("ERROR: Could not send data package to link\n");
		free(package);
        exit(ERROR);
	}

	free(package);
}

void sendFile(char *fileName, int fd)
{

    //Open file
    FILE *file = openFile(fileName);

    //File size
    int fileS = fileSize(file);
    char fileSizeBuf[sizeof(int) * 3 + 2];
    snprintf(fileSizeBuf, sizeof fileSizeBuf, "%d", fileS);

    //Start Packet
    sendControl(fd, CTRL_START, fileSizeBuf, fileName);

    //File buffer
    int maxSize = 256;
    char *fileBuf = malloc(maxSize);

    //Read chunks
    int readBytes = 0, i = 0;
    while ((readBytes = fread(fileBuf, sizeof(char), maxSize, file)) > 0)
    {
        //Send data packet
        sendData(fd, (i++) % 255, fileBuf, readBytes);

        //Reset file buffer
        fileBuf = memset(fileBuf, 0, maxSize);
    }

    free(fileBuf);

    //Close file
    if (fclose(file) != 0) {
		printf("ERROR: Unable to close file.\n");
		exit(ERROR);
	}

    //End Packet
    sendControl(fd, CTRL_END, "0", "");

}