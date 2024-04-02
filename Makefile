# Makefile for compiling battle.c
CC = gcc
CFLAGS = -Wall -g
LDFLAGS =
PORT = 58833

# Executable file name
TARGET=battle

# Source and object files
SRC = battle.c
OBJ = $(SRC:.c=.o)

# Rule to make target
$(TARGET): $(OBJ)
    $(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

# Rule for object files
%.o: %.c
    $(CC) $(CFLAGS) -DPORT=$(PORT) -c $< -o $@

# Clean files
clean:
    rm -f $(TARGET) $(OBJ)

.PHONY: clean