# Compiler to use
CC=gcc

# Compiler flags
CFLAGS=-g -Wall

# Port number for the server to listen on
PORT=58833

# Name of your final executable
TARGET=battle

# Source files
SRC=battle.c

# Object files
OBJ=$(SRC:.c=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -DPORT=$(PORT) -o $@ $^

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -DPORT=$(PORT) -c $<

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
