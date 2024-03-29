/*Non-Canonical Input Processing*/

#include < sys / types.h >
#include < sys / stat.h >
#include < fcntl.h >
#include < termios.h >
#include < stdio.h >
#include < stdlib.h >
#include < signal.h >
#include < unistd.h >

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

int timeout = 0;

void alarmTimeout(){
	printf("Time out reached, sending again");
	timeout = 1;
}

void LLOPEN(int * fd)
{

	signal(SIGALRM, alarmTimeout);

	const int size = 5;
	char trama_inf[size];

	// Setup trama_inf
	trama_inf[0] = 0x7e;
	trama_inf[1] = 0x03;
	trama_inf[2] = 0x03;
	trama_inf[3] = trama_inf[1] ^ trama_inf[2];
	trama_inf[4] = 0x7e;

	//Write command
	int res = write(* fd, trama_inf, size);
	printf("Command sent, waiting response...\n");
	fflush(NULL);

	//Wait for command
	char buf[5];
	int count = 0;
	//TODO alarm not working
	alarm(1);
	while (timeout == 0) {
		if ((res = read((* fd), buf + count, 1)) > 0) {
			count += res;
			if (buf[count - 1] == '\0')
				timeout = 1;
		}
	}

	//Print command received
	printf("Received command response:\n");
	for (int i = 0; i < size; ++i)
	printf("%x\n", buf[i] & 0xff);

}

void LLWRITE()
{
}

int main(int argc, char ** argv)
{

	int fd;
	struct termios oldtio, newtio;

	if ((argc < 2) ||
		((strcmp("/dev/ttyS0", argv[1]) != 0) &&
			(strcmp("/dev/ttyS1", argv[1]) != 0))) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

	/* 
	  Open serial port device for reading and writing and not as controlling tty 
	  because we don't want to get killed if linenoise sends CTRL-C. 
	*/

	fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(argv[1]);
		exit(-1);
	}
	if (tcgetattr(fd, & oldtio) == -1) { /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(& newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
	newtio.c_cc[VMIN] = 5;  /* blocking read until 5 chars received */

	/*  
	  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a  
	  leitura do(s) próximo(s) caracter(es) 
	*/

	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, & newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	fflush(NULL);

	LLOPEN(& fd);

	if (tcsetattr(fd, TCSANOW, & oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
	return 0;
}
