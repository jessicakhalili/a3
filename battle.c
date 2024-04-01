#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifndef PORT
    #define PORT 58833
#endif

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct client {
  int fd;
  struct in_addr ipaddr;
  struct client *next;
  int state; //0:waiting name 1:name typed, waiting match 2:non-active 3:active
  *char name;
  int hp;
  int powermove;
  int prev;
  struct client *opponent;
  int say;
};

static int searchmatch(struct client *p);
static void movetoend(int fd);
static struct client *addclient(int fd, struct in_addr addr);
static struct client *removeclient(int fd);
static void broadcast(struct client *first, char *s, int size);
int handleclient(struct client *p, struct client *first);


int bindandlisten(void);


int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *first = NULL;
    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;

    int i;


    srand((unsigned int)time(NULL)); //seed rand()


    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){ //client wants to connect to server
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            addclient(head, clientfd, q.sin_addr);
            char *namemsg = "What is your name?\r\n"; 
            write(clientfd, namemsg, strlen(namemsg)); //Add client, print name message, and set client state to 0. 
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) { 
                for (p = first; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p);  //all actions from the client will be taken care of in handle client function.
                        if (result == -1) {  // if client closed, remove from client list and fd_set.
                            int tmp_fd = p->fd;
                            removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}


int handleclient(struct client *p) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {


      if (p->say == 1) { //priority check: if say flag is 1, print msg to opponent
          write(p->opponent, buf, strlen(buf));
          p->say = 0;
          return 1;
      }



      if ((p->state) == 0) {  //if this client hasnt typed name yet, input becomes name and change state to 1. Open a match if possible.
        p->name = buf;
        p->state = 1;
        sprintf(outbuf, "Welcome, %s! Awaiting opponent...\r\n", p->name);
        write(p->fd, outbuf, strlen(outbuf));
        
        sprintf(outbuf, "**%s enters the arena**\r\n", p->name); //print msg to all clients
        broadcast(first, outbuf, strlen(outbuf))

        searchmatch(p);
        return 1;
      }
      else if ((p->state) == 1) {  //this client is waiting for a match, all inputs will be ignored.
        return 1;
      }
      else if ((p->state) == 2) {  //this client is in a battle, in defense, all inputs will be ignored.
        return 1;
      }
      else if ((p->state) == 3) {  //this client is in a battle, in offense, perform the action.
        struct client *t = p->opponent;
        if (strcmp(buf, "a")) {
          int r = 2 + rand() % 5;
          t->hp -= r;

          if (t->hp <= 0) {
            //print end message
            sprintf(outbuf, "You hit %s for %d damage!\n%s gives up. You win!\n\nAwaiting next opponent...\n\r\n", t->name, r, t->name);
            write(p->fd, outbuf, strlen(outbuf));

            sprintf(outbuf, "%s hits you for %d damage!\nYou are no match for %s. You scurry away...\n\nAwaiting next opponent...\n\r\n", p->name, r, p->name);
            write(t->fd, outbuf, strlen(outbuf));
            // set previously matched opponents
            p->prev = t;
            t->prev = p;
            //send them to end of client list
            movetoend(t->fd);
            movetoend(p->fd);
            searchmatch(t);
            searchmatch(p);
            return 1;
          }

          //battle continues
          sprintf(outbuf, "You hit %s for %d damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for abc to strike...\r\n", t->name, r, p->hp, p ->powermove, t->name, t->hp);
          write(p->fd, outbuf, strlen(outbuf));
          
          if (t->powermove > 0) {
            sprintf(outbuf, "%s hits you for %d damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(p)owermove\n(s)peak something\n\r\n", p->name, r, t->hp, t->powermove, p->name,
             p->hp);
            write(t->fd, outbuf, strlen(outbuf));
          }
          else if (t->powermove == 0) {
            sprintf(outbuf, "%s hits you for %d damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(s)peak something\n\r\n", p->name, r, t->hp, t->powermove, p->name, p->hp);
            write(t->fd, outbuf, strlen(outbuf));
          }
          p->state = 2;
          t->state = 3;
          return 1; 
        }
        else if (strcmp(buf, "p")) {
          //if p == 0 ignore, else carry out the action, hit rate 50%, dmg = (2~6) * 3, if hp drops to 0 perform line 145-161

        }
        else if (strcmp(buf, "s")) {
          //set say flag to 1.  
          p->say = 1;
          return 1;
        }
      }
      return 1;
    } 

    else if (len <= 0) {
        // socket is closed
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        if ((p->state) > 0) {  //if this client has typed name, broadcast leaving msg to all clients
            sprintf(outbuf, "**%s leaves**\r\n", p->name);
            broadcast(first, outbuf, strlen(outbuf));
        }
        return -1;
    } 
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static void addclient(int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = NULL;
    p->state = 0;
    p->hp = NULL;
    p->powermove = NULL;
    p->prev = NULL;
    p->opponent = NULL;
    p->say = 0;

    struct client **p2;
    struct client **prev2;
    for (p2 = &first; *p2; p2 = &(*p2)->next) {
      prev2 = p2;
    }
    (*prev2)->next = p;
}

static void client *removeclient(int fd) {
    struct client **p;

    for (p = &first; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    if (*p) {
        if (p == &first) { //if fd is first position (special case)
          
          struct client **p2;
          struct client **prev2;
          for (p2 = &first; *p2; p2 = &(*p2)->next) {
            prev2 = p2;
          }
          if (prev2 == p) { //if fd is the one and only client in the list, remove and update first to NULL.
            free(*p);
            first = NULL;
          }
          else { //else, update first and perform task.
            first = &(*p)->next;
            free(*p);
          }
        }
        else {
          struct client *t = (*p)->next;
          printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
          free(*p);
          *p = t;
        }
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

static void movetoend(int fd) {
    struct client **p;
    struct client **prev;

    for (p = &first; *p && (*p)->fd != fd; p = &(*p)->next) {
      prev = p;
    }

    if (*p) {
        p->state = 1;  //match is over, reset some attributes
        p->hp = NULL;
        p->powermove = NULL;
        p->opponent = NULL;
        if (p == &first) { //if fd is first position (special case)
          
          struct client **p2;
          struct client **prev2;
          for (p2 = &first; *p2; p2 = &(*p2)->next) {
            prev2 = p2;
          }
          if (prev2 == p) { //if fd is the one and only client in the list, do nothing.
            break;
          }
          else { //else, update the variable first and perform the task.
            first = &(*p)->next;
          struct client *t = *p;
          (*t)->next = (*prev2)->next;
          (*prev2)->next = *t;
          }
        }
        else {  //for all other cases (including the case of 2 clients in list with fd being the second), perform the task.
          struct client *t = *p;
          (*prev)->next = (*p)->next;

          struct client **p2;
          struct client **prev2;
          for (p2 = &first; *p2; p2 = &(*p2)->next) {
            prev2 = p2;
          }
          (*t)->next = (*prev2)->next;
          (*prev2)->next = *t;
        }
    } 
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

static int searchmatch(struct client *p) {
  struct client *p1;
        for (p1 = first; p1 != NULL; p1 = p1->next) {  //search if theres available client for match
          if ((p1->state) == 1) {
            if (p1 != p && (p->prev != p1->fd)) { // if there exists two different clients (not previously matched), start a match
                int n = rand() % 2;
                if (n == 0) {  //random attack,defense
                  p->state = 3;
                  p1->state = 2;
                  p->hp = 20 + rand() % 11;
                  p->powermove = 1 + rand() % 3;
                  p1->hp = 20 + rand() % 11;
                  p1->powermove = 1 + rand() % 3;
                  p->opponent = p1;
                  p1->opponent = p;

                  sprintf(outbuf, "You engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(p)owermove\n(s)peak something\n\r\n", p1->name, p->hp, p->powermove, p1->name, p1->hp);
                  write(p->fd, outbuf, strlen(outbuf));

                  sprintf(outbuf, "You engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for abc to strike...\r\n", p->name, p1->hp, p1 ->powermove, p->name, p->hp);
                  write(p1->fd, outbuf, strlen(outbuf));
                  return 1;
                }
                else {
                  p1->state = 3;
                  p->state = 2;
                  p1->hp = 20 + rand() % 11;
                  p1->powermove = 1 + rand() % 3;
                  p->hp = 20 + rand() % 11;
                  p->powermove = 1 + rand() % 3;
                  p->opponent = p1;
                  p1->opponent = p;

                  sprintf(outbuf, "You engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(p)owermove\n(s)peak something\n\r\n", p->name, p1->hp, p1->powermove, p->name, p->hp);
                  write(p1->fd, outbuf, strlen(outbuf));

                  sprintf(outbuf, "You engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for abc to strike...\r\n", p1->name, p->hp, p ->powermove, p1->name, p1->hp);
                  write(p->fd, outbuf, strlen(outbuf));
                  return 1;
                }
            }
          }
        }
        return 1; // Tried to create match but failed. Remain waiting for opponent.
}


static void broadcast(struct client *first, char *s, int size) {
    struct client *p;
    for (p = first; p; p = p->next) {
        write(p->fd, s, size);
    }
    /* should probably check write() return value and perhaps remove client */
}