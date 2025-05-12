CC=gcc
CFLAGS=-O0 -g -pipe
CFLAGS+=$(shell python3-config --cflags)
CFLAGS+=$(shell pkg-config --cflags)

LDFLAGS+=$(shell python3-config --ldflags --embed)
LDLAGS+=$(shell pkg-config --libs libsystemd-journal)
# Fedora
LDFLAGS+=$(shell pkg-config --libs libsystemd)

objects=spython.o

all: spython

%.o: %.c
	$(CC) -c $< $(CFLAGS)

spython: spython.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf *.o spython
