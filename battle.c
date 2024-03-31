#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
#define PORT 30100 // default port
#endif

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct client {
  int fd;
  struct in_addr ipaddr;
  struct client *next;
};

int bindandlisten(void) {
  struct sockaddr_in server_addr;
  int listenfd, optval = 1;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "socket creation failed");
    exit(EXIT_FAILURE);
  }

  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = INADDR_ANY;
      server_addr.sin_port = htons(PORT);
  
      if (bind(listenfd, (struct sockaddr )&server_addr, sizeof(server_addr)) < 0) {
          fprintf(stderr, "bind failed");
          exit(EXIT_FAILURE);
      }
  
      if (listen(listenfd, MAX_CLIENTS) < 0) {
          fprintf(stderr, "listen failed");
          exit(EXIT_FAILURE);
      }
  
      return listenfd;
  }

int main(void) {
    int listenfd = bindandlisten(), newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(listenfd, (struct sockaddr)&cli_addr, &clilen);
        if (newsockfd < 0) {
            fprintf(stderr, "error on accept");
            continue;
        }

        printf("New connection from %s\n", inet_ntoa(cli_addr.sin_addr));

        // Add the new connection to your list of clients here using select() or similar to manage multiple connections.
        // Close the socket for now.
        close(newsockfd);
    }

    close(listenfd);
    return 0;
}
