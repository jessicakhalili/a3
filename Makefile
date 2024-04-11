CC=gcc
CFLAGS=-g -Wall -DPORT=$(PORT)
PORT=58833

all: battle

battle: battle.c
    $(CC) $(CFLAGS) -o battle battle.c

clean:
    rm -f battle