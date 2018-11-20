#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <netinet/in.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"
#define STRING_LENGTH 50
#define SOCKET_SIZE 1000

int main(int argc, char **argv){

  int socketfd;
	int socketfdClient = -1;
	struct sockaddr_in server_addr;
  struct sockaddr_in server_addr_client;
  char responseCode[3];

  //Info strings
  char user[STRING_LENGTH];
  char pw[STRING_LENGTH];
  char host[STRING_LENGTH];
  char path[STRING_LENGTH];
  char file[STRING_LENGTH];

  //Reset strings
  memset(user, 0, STRING_LENGTH);
  memset(pw,0,STRING_LENGTH);
  memset(host,0,STRING_LENGTH);
  memset(path,0,STRING_LENGTH);
  memset(file,0,STRING_LENGTH);

  //Parse arguments
  strcat(user, argv[1]);
  strcat(pw, argv[2]);
  strcat(host, argv[3]);
  strcat(path, argv[4]);

  //Parse file
  int indexPath = 0;
  int indexFile = 0;
  char c;
  do {
    c = path[indexPath];
    if(c == '/'){
      indexFile = 0;
    }else{
      file[indexFile] = c;
      indexFile++;
    }
    indexPath++;
  } while(c != '\0');
  file[indexFile++] = '\0';

  printf("\n%s\n",file);

  return 0;
}
