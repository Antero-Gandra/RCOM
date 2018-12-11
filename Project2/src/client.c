#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <ctype.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"

struct Arguments
{
  char user[50];
  char password[50];
  char host[50];
  char path[50];
} arguments;

//Parse Arguments
int parseArguments(int argc, char **argv)
{

  //Wrong argument size
  if (argc < 2)
    return 1;

  //Raw string
  char *raw = argv[1];

  //Process string
  char c;
  //Global index
  int i = 6;
  //Local index
  int li = 0;
  int state = 0;
  do
  {
    c = raw[i];
    switch (state)
    {
    case 0:
      if (c == ':')
      {
        arguments.user[li] = '\0';
        state++;
        li = 0;
        break;
      }
      arguments.user[li] = c;
      li++;
      break;
    case 1:
      if (c == '@')
      {
        arguments.password[li] = '\0';
        state++;
        li = 0;
        break;
      }
      arguments.password[li] = c;
      li++;
      break;
    case 2:
      if (c == '/')
      {
        arguments.host[li] = '\0';
        state++;
        li = 0;
        break;
      }
      arguments.host[li] = c;
      li++;
      break;
    case 3:
      if (c == '\0')
      {
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
  } while (c != '\0');

  if (state < 4)
    return 1;

  return 0;
}

//Get ip address according to the host's name
struct hostent *getip(char *host)
{
  struct hostent *h;

  if ((h = gethostbyname(host)) == NULL)
  {
    herror("gethostbyname");
    exit(1);
  }

  return h;
}

//Get response code
int response(int socket, char *code)
{
  int state = 0;
  int i = 0;
  char c;

  printf("Response: ");

  while (state != 3)
  {
    if (read(socket, &c, 1) == -1)
      return -1;

    printf("%c", c);

    switch (state)
    {
    //3 digit number
    case 0:
      if (c == ' ')
      {
        if (i != 3)
        {
          printf(" > Error receiving response code\n");
          return -1;
        }
        i = 0;
        state = 1;
      }
      else
      {
        if (c == '-')
        {
          state = 2;
          i = 0;
        }
        else
        {
          if (isdigit(c))
          {
            code[i] = c;
            i++;
          }
        }
      }
      break;
    //Read until end of line
    case 1:
      if (c == '\n')
      {
        state = 3;
      }
      break;
    //Multi line code
    case 2:
      if (c == code[i])
      {
        i++;
      }
      else
      {
        if (i == 3 && c == ' ')
        {
          state = 1;
        }
        else
        {
          if (i == 3 && c == '-')
          {
            i = 0;
          }
        }
      }
      break;
    }
  }

  return 0;
}

//  Example
//  ./download ftp://utilizador:pass@ftp.fe.up.pt/pasta1/pasta2/imagem.png

//TODO separate to functions most stuff
int main(int argc, char **argv)
{

  //Parse arguments
  if (parseArguments(argc, argv) == 1)
  {
    printf("\nInvalid arguments");
    printf("\nUsage example:");
    printf(" download ftp://[<user>:<password>@]<host>/<url-path>\n");
  }

  //Print information to be used
  printf("Username: %s\n", arguments.user);
  printf("Password: %s\n", arguments.password);
  printf("Host: %s\n", arguments.host);
  printf("Path: %s\n", arguments.path);

  //Get IP
  struct hostent *h = getip(arguments.host);

  //Print IP
  printf("\nIP Address: %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

  //Handle address
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr)));
  server_addr.sin_port = htons(SERVER_PORT);

  //Open TCP socket
  int socketfd;
  if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket()");
    exit(0);
  }

  //Connect to Server
  if (connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("connect()");
    exit(0);
  }

  //Read response
  char code[3];
  if (response(socketfd, code) < 0)
  {
    perror("response()");
    exit(0);
  }

  //Confirm response
  if (code[0] == '2')
  {
    printf("Connection Estabilished\n");
  }

  //TODO Send username
  //TODO Send password
  //TODO Get server port to be used
  //TODO Open TCP socket to that port
  //TODO Connect to server
  //TODO Create and get file
  //TODO Close connection

  return 0;
}
