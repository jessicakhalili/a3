#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

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
  char* name;
  int hp;
  int powermove;
  int prev;
  struct client *opponent;
  int say;
};

static int searchmatch(struct client *p);
static void movetoend(int fd);
// static struct client *addclient(int fd, struct in_addr addr);
void removeclient(int fd);
static void broadcast(struct client *first, char *s, int size);
// int handleclient(struct client *p, struct client *first);


int bindandlisten(void);

// Global Variables, fix names carefully
struct client *first = NULL;
char canonbuf[64] = ""; // Buffer for commands that require canonical mode

void handle_alarm(int sig) {
  struct client *p = first;
  while (p != NULL) {
    if (p->state == 3) { // active player
      p->state = 2; // switch to non-active
      char *message = "You took too long to attack. Your turn is skipped.\n";
      write(p->fd, message, strlen(message));
      if (p->opponent != NULL) {
        write(p->opponent->fd, message, strlen(message));
      }
      break;
    }
    p = p->next;
  }
}

void start_turn_timer(struct client *p) {
    signal(SIGALRM, handle_alarm);
    alarm(30); // start a 30-second timer
    instructions(p, 0); // print instructions and set player's state to 3 (attacker)
}

void attack(struct client *p, int damage) {
    alarm(0); // cancel the timer if the player attacks within the time limit
    damagemessage(p, damage); // send damage message
    // check if the opponent's hitpoints are 0 or less, if so, end the match
    if (p->opponent->hp <= 0) {
        endofmatch(p);
    }
    else {
        // if the opponent is still alive, start their turn
        start_turn(p->opponent);
    } 
}

void damagemessage(struct client *p, int r){ // A helper function that takes care of damage instructions
	char outbuf[512];
	struct client *t = p->opponent;
	// Message the opponent
	sprintf(outbuf, "\nYou hit %s for %d damage!\r\n", t->name, r);
	write(p->fd, outbuf, strlen(outbuf));
	// Message the client
	sprintf(outbuf, "%s hits you for %d damage!\r\n", p->name, r);
	write(t->fd, outbuf, strlen(outbuf));
}

void instructions(struct client *p, int switchrole){ // A helper function that takes care of print instructions
	  char outbuf[512];
	  struct client *t = p->opponent;
          sprintf(outbuf, "Your hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\r\n", p->hp, p ->powermove, t->name, t->hp, t->name);
          write(p->fd, outbuf, strlen(outbuf));
          if (t->powermove > 0) {
            sprintf(outbuf, "Your hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(p)owermove\n(s)peak something\n\r\n", t->hp, t->powermove, p->name,
             p->hp);
            write(t->fd, outbuf, strlen(outbuf));
          }
          else if (t->powermove == 0) {
            sprintf(outbuf, "Your hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\n(a)ttack\n(s)peak something\n\r\n", t->hp, t->powermove, p->name, p->hp);
            write(t->fd, outbuf, strlen(outbuf));
          }
	  if (switchrole == 1){ // If true, players will not switch roles (attacker and defender role)
	    return;
	  }
          p->state = 2;
          t->state = 3;
          start_turn_timer(p->opponent);
}

void endofmatch(struct client *p){ // A helper function that takes care of the end of match cleanup
	char outbuf[512];
	struct client *t = p->opponent;
	//print end message
	sprintf(outbuf, "%s gives up. You win!\n\nAwaiting next opponent...\n\r\n", t->name);
	write(p->fd, outbuf, strlen(outbuf));

        sprintf(outbuf, "You are no match for %s. You scurry away...\n\nAwaiting next opponent...\n\r\n", p->name);
        write(t->fd, outbuf, strlen(outbuf));
        // set previously matched opponents
        p->prev = t->fd;
        t->prev = p->fd;
        //send them to end of client list
        movetoend(t->fd);
        movetoend(p->fd);
        searchmatch(t);
        searchmatch(p);
}

int handleclient(struct client *p) {
    char buf[256]; // Buffer to store input from the client.
    char outbuf[512]; // Buffer to construct output messages to be sent to clients.
    int len = read(p->fd, buf, sizeof(buf) - 1); // Read input from the client's socket into buf, limiting read size to one less than buffer size to leave space for null terminator.

    if (len > 0) { // If the read is successful and data is received:
      buf[len] = '\0'; // Null-terminate the received input and make it a valid C string.

      if (p->say == 1) { //priority check: if say flag is 1, print msg to opponent | If the 'say' flag is set (indicating the client is in a "speak" state)...
          if (strchr(buf, '\n') == NULL) { // Client is not done typing a message
	    strcat(canonbuf, buf);
	    return 1;
	  }
	  strcat(canonbuf, buf); // For readability
	  sprintf(outbuf, "\nYou got a message from %s!\r\n", p->name);
	  write(p->opponent->fd, outbuf, strlen(outbuf));
	  write(p->opponent->fd, canonbuf, strlen(canonbuf)); // Send received message directly to the opponent.
          strcpy(canonbuf, ""); // Reset the canonical buffer
	  p->say = 0; // Reset the 'say' flag.
	  instructions(p->opponent, 1); // Print instructions again without switching roles
          return 1; // Exit the function, indicating success.
      }

      if ((p->state) == 0) {  //if this client hasnt typed name yet, input becomes name and change state to 1. Open a match if possible.
	if (strchr(buf, '\n') == NULL) { // Client has not finished typing their name
	  strcat(canonbuf, buf);
	  return 1;
	}
	*(strchr(buf, '\n')) = '\0';
	strcat(canonbuf, buf);
	p->name = strdup(canonbuf); // Copies buffer's contents to a new memory location that doesn't get overwritten
	strcpy(canonbuf, ""); // Reset the canonical buffer
	p->state = 1;
        sprintf(outbuf, "Welcome, %s! Awaiting opponent...\r\n", p->name);
        write(p->fd, outbuf, strlen(outbuf));

        sprintf(outbuf, "**%s enters the arena**\r\n", p->name); //print msg to all clients
        broadcast(first, outbuf, strlen(outbuf));

        searchmatch(p);
        return 1;
      }
      else if ((p->state) == 1 || (p->state) == 2) {  //this client is waiting for a match or in defense, all inputs will be ignored.
        return 1;
      }
      else if ((p->state) == 3) {  //this client is in a battle, in offense, perform the action.
        struct client *t = p->opponent;
        if (strcmp(buf, "a") == 0 || strcmp(buf, "a\n") == 0) { // strcmp returns 0 if the strings are equal
          int r = 2 + rand() % 5;
          t->hp -= r;
	  // Print damage message
	  damagemessage(p, r);
          if (t->hp <= 0) {
	    endofmatch(p);
            return 1;
          }

          //battle continues
	  instructions(p, 0);
          return 1;
        }
        else if (strcmp(buf, "p") == 0 || strcmp(buf, "p\n") == 0) {
          //if powermove == 0 ignore, else carry out the action, hit rate 50%, dmg = (2~6) * 3, if hp drops to 0 end match
		if ((p->powermove) > 0) {
			p->powermove -= 1;
			if (rand() % 2) {
				int dmg = 3 * (2 + rand() % 5);
				t->hp -= dmg;
				damagemessage(p, dmg);
				if (t->hp <= 0) {
					endofmatch(p);
					return 1;
				}
				instructions(p, 0);
				return 1;
			}
			// if missed send 0 as the damage
			damagemessage(p, 0);
			instructions(p, 0);
			return 1;
		}
        }
        else if (strcmp(buf, "s") == 0 || strcmp(buf, "s\n") == 0) {
	  sprintf(outbuf, "\n\nPlease type the message you want to send.\nWhen you're finished, hit enter.\r\n");
	  write(p->fd, outbuf, strlen(outbuf));
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
    return 0;
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
    p->hp = 0;
    p->powermove = 0;
    p->prev = 0;
    p->opponent = NULL;
    p->say = 0;

    if (first == NULL) {
	first = p;
	return;
    }

    struct client **p2;
    struct client **prev2;
    for (p2 = &first; *p2; p2 = &(*p2)->next) {
      prev2 = p2;
    }
    (*prev2)->next = p;
}

void removeclient(int fd) {
    struct client **p; // pointer to pointer for traversal and modification.
    struct client *opponent;
    int oppflag = 0;
    char outbuf[512];
    // loop through the list to find the client with the matching file descriptor.
    for (p = &first; *p && (*p)->fd != fd; p = &(*p)->next);

    // check if the client was found.
    if (*p) {
        // special case handling is not required for the first element, it's covered by general logic.
        struct client *temp = *p; // temporarily store the pointer to the client to be removed.
	// Check if the client is in a match
	if ((*p)->state == 2 || (*p)->state == 3) {
		opponent = (*p)->opponent;
		oppflag = 1;
	}

        // logging the removal action.
        printf("Removing client %d %s\n", fd, inet_ntoa(temp->ipaddr));

        // advance the pointer to remove the current client from the list.
        *p = temp->next;

        // free the removed client's dynamically allocated name, if applicable.
        if (temp->name) {
            free(temp->name);
        }

        // finally, free the client struct itself.
        free(temp);

	// End the match for the opponent
	if (oppflag == 1){
		sprintf(outbuf, "\nYour opponent has fled the battlefield. Ending the match...\r\n");
		write(opponent->fd, outbuf, strlen(outbuf));
		movetoend(opponent->fd);
		searchmatch(opponent);
	}
    } else {
        // if the client with the specified file descriptor was not found in the list.
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
    }
}

static void movetoend(int fd) {
    struct client **p;
    struct client **prev;

    for (p = &first; *p && (*p)->fd != fd; p = &(*p)->next) {
      prev = p;
    }

    if (*p) {
        (*p)->state = 1;  //match is over, reset some attributes
        (*p)->hp = 0;
        (*p)->powermove = 0;
        (*p)->opponent = NULL;
        if (p == &first) { //if fd is first position (special case)

          struct client **p2;
          struct client **prev2;
          for (p2 = &first; *p2; p2 = &(*p2)->next) {
            prev2 = p2;
	  }
	  if (prev2 != p) { //if fd is the one and only client in the list, do nothing.
       	  //else, update the variable first and perform the task.
	    struct client *tempfirst = ((*p)->next); // A temporary variable to hold the address of *p->next
            struct client *templast = *prev2;
	    struct client *t = *p;
            t->next = (*prev2)->next;
            templast->next = t;
	    first = tempfirst;
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
          t->next = (*prev2)->next;
          (*prev2)->next = t;
        }
    }
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

static int searchmatch(struct client *p) {
  char outbuf[512];
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

                  sprintf(outbuf, "You engage %s!\r\n", p1->name);
                  write(p->fd, outbuf, strlen(outbuf));

                  sprintf(outbuf, "You engage %s!\r\n", p->name);
                  write(p1->fd, outbuf, strlen(outbuf));
		  instructions(p1, 1);
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

                  sprintf(outbuf, "You engage %s!\r\n", p->name);
                  write(p1->fd, outbuf, strlen(outbuf));

                  sprintf(outbuf, "You engage %s!\r\n", p1->name);
                  write(p->fd, outbuf, strlen(outbuf));
		  instructions(p, 1);
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

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
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
            addclient(clientfd, q.sin_addr);
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
                            removeclient(p->fd);
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
