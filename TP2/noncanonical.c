/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>

// ./nc /dev/ttyS0

/*
Usar fflush(NULL) no inicio e depois de invocar o write. Evita chars estranhos no print da consola, limpa os buffers
*/ 

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

void LLOPEN(int* fd){

	int res;
	char buf[5];
	int count = 0;
	printf("Waiting...\n");
    while (STOP==FALSE) {
		if((res = read((*fd),buf+count,1)) > 0) {
			count += res;
			if (buf[count-1]=='\0') STOP=TRUE;
		}		
    }

    printf("Received string:\n");
    for (int i = 0; i < 5; ++i)
    	printf("%x\n",buf[i] & 0xff);

}

void LLCLOSE(){

}

void LLREAD(){

}

int main(int argc, char** argv)
{
    int fd,c;
    struct termios oldtio,newtio;

    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0))) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 1;
    newtio.c_cc[VMIN]     = 0;

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    //Set tcs with new struct
    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    //LLOPEN(&fd);

	//Testing old system
    int count = 0;
    int res = 0;
    char buf[255];
    while (STOP==FALSE) {
		if((res = read(fd,buf+count,1)) > 0) {
			count += res;
			if (buf[count-1]=='\0') STOP=TRUE;
		}		
    }

    printf("%s\n", buf);

    //Reset tcs with original struct and close fd
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    

    return 0;
}
