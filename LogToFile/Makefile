CC=gcc
CFLAGS=-O0 -g -pipe
CFLAGS+=$(shell python3.8-config --cflags)

LDFLAGS+=$(shell python3.8-config --ldflags --embed)

objects=spython.o

all: spython

%.o: %.c
	$(CC) -c $< $(CFLAGS)

spython: spython.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf *.o spython
