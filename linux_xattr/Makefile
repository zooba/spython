CC=gcc
CFLAGS=-O0 -g -pipe
CFLAGS+=$(shell python3.8-config --cflags)
CFLAGS+=$(shell pkg-config libcrypto --cflags)
CFLAGS+=$(shell pkg-config libseccomp --cflags)

LDFLAGS+=$(shell python3.8-config --ldflags --embed)
LDFLAGS+=$(shell pkg-config libcrypto --libs)
LDFLAGS+=$(shell pkg-config libseccomp --libs)

objects=spython.o

all: spython

%.o: %.c
	$(CC) $(CFLAGS) -c $<

spython: spython.o
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -rf *.o spython