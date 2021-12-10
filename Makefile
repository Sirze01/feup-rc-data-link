CC = gcc 
CFLAGS = -Wall
PROG_NAME = application

SRCDIR = src

SOURCES := $(shell find $(SRCDIR) -name '*.c')
OBJECTS := $(SOURCES:%.c=%.o)

.PHONY: clean virtual

all: application

application: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(PROG_NAME) $(OBJECTS)

ifdef BAUDRATE
src/protocol/data_link.o: CFLAGS+=-D BAUDRATE=$(BAUDRATE)
endif
ifdef CONNECTION_TIMEOUT_TS
src/data_link/data_link.o: CFLAGS+=-D CONNECTION_TIMEOUT_TS=$(CONNECTION_TIMEOUT_TS)
endif
ifdef CONNECTION_MAX_TRIES
src/data_link/data_link.o: CFLAGS+=-D CONNECTION_MAX_TRIES=$(CONNECTION_MAX_TRIES)
endif
ifdef INDUCED_FER
src/data_link/data_link.o: CFLAGS+=-D INDUCED_FER=$(INDUCED_FER)
endif
ifdef INDUCED_DELAY_US
src/data_link/data_link.o: CFLAGS+=-D INDUCED_DELAY_US=$(INDUCED_DELAY_US)
endif

virtual:
	sudo socat -d  -d  PTY,link=/dev/ttyS10,mode=777   PTY,link=/dev/ttyS11,mode=777

clean:
	rm -f $(PROG_NAME) $(OBJECTS)