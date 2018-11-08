#pragma once

typedef enum
{
    CTRL_START = 1,
    CTRL_DATA = 2,
    CTRL_END = 3
} AppControlType;

typedef enum
{
    FILE_SIZE,
    FILE_NAME
} Parameter;

FILE *openFile(char *fileName);
int fileSize(FILE *file);

void sendControl(int fd, int cmd, char *fileS, char *fileName);
void sendData(int fd, int N, const char* buffer, int length);

void sendFile(char *fileName, int fd);

char* receiveControl(int fd, int* controlPackageType, int* fileLength);
void receiveData(int fd, int* N, char** buf, int* length);

void receiveFile(int fd);