CC = gcc 
CFLAGS = -Wall
PROG_NAME = application

SRCDIR = src

SOURCES := $(shell find $(SRCDIR) -name '*.c')
OBJECTS := $(SOURCES:%.c=%.o)

.PHONY: clean

application: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROG_NAME) $(OBJECTS)

clean:
	rm -f $(PROG_NAME) $(OBJECTS)