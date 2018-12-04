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

struct Arguments {
   char  user[50];
   char  password[50];
   char  host[50];
   char  path[50];
} arguments;

int parseArguments(int argc, char **argv){

  //Wrong argument size
  if(argc < 2)
    return 1;

  //Raw string
  char* raw = argv[1];

  //Process string
  char c;
  //Global index
  int i = 7;
  //Local index
  int li = 0;
  int state = 0;
  do{
    c = raw[i];
    switch(state){
      case 0:
        if(c == ':'){
          arguments.user[li] = '\0';
          state++;
          li = 0;
          break;
        }
        arguments.user[li] = c;
        li++;
        break;
      case 1:
        if(c == '@'){
          arguments.password[li] = '\0';
          state++;
          li = 0;
          //Because of ']'
          i++;
          break;
        }
        arguments.password[li] = c;
        li++;
        break;
      case 2:
        if(c == '/'){
          arguments.host[li] = '\0';
          state++;
          li = 0;
          break;
        }
        arguments.host[li] = c;
        li++;
        break;
      case 3:
        if(c == '\0'){
          arguments.path[li] = '\0';
          state++;
          break;
        }
        arguments.path[li] = c;
        li++;
        break;
      default:
        break;
    }
    i++;
  }while(c != '\0');

  if(state < 4)
    return 1;

  return 0;

}

int main(int argc, char **argv){

  if(parseArguments(argc, argv) == 1){
    printf("\nInvalid arguments");
    printf("\nUsage example:");
    printf(" ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
  }

  printf("%s\n", arguments.user);
  printf("%s\n", arguments.password);
  printf("%s\n", arguments.host);
  printf("%s\n", arguments.path);

  int socketfd;
	int socketfdClient = -1;
	struct sockaddr_in server_addr;
  struct sockaddr_in server_addr_client;
  char responseCode[3];

  return 0;
}
