#pragma once

typedef enum
{
    CTRL_START = 1,
    CTRL_DATA = 2,
    CTRL_END = 3
} AppControlType;

typedef enum
{
    FILE_NAME,
    FILE_SIZE
} Parameter;

FILE *openFile(char *fileName);
int fileSize(FILE *file);

void sendControl(int fd, int cmd, char *fileS, char *fileName);
void sendData(int fd, int N, const char* buffer, int length);

void sendFile(char *fileName, int fd);