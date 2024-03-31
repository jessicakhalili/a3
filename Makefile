# Default port number for now; override this by calling `make PORT=<number>`
PORT=30100

# Compiler flags
CFLAGS= -DPORT=$(PORT) -g -Wall

# The compiler to use
CC=gcc

# Target executable name
TARGET=battle

# Object files to build
OBJS=battle.o

# Compile the battle server
$(TARGET): $(OBJS)
    $(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile battle.c to an object file
battle.o: battle.c
    $(CC) $(CFLAGS) -c battle.c

# Clean up compiled files
clean:
    rm -f $(TARGET) $(OBJS)

.PHONY: clean
