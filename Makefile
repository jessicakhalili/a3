# Define the default port number
PORT_DEFAULT = 58833

# Set the port number to the default value if not provided
PORT ?= $(PORT_DEFAULT)

# Compiler flags
CFLAGS = -DPORT=$(PORT) -g -Wall

# Source files
SRCS = battle.c

# Object files
OBJS = $(SRCS:.c=.o)

# Dependency files
DEPS = $(SRCS:.c=.d)

# Compiler
CC = gcc

# Target executable
TARGET = battle

# Phony targets
.PHONY: all clean

# Default target
all: $(TARGET)

# Linking object files to generate the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Compilation rule for C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files
-include $(DEPS)

# Generate dependency files
%.d: %.c
	$(CC) $(CFLAGS) -MM $< -MF $@ -MT '$(basename $@).o $@'

# Clean target
clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)
