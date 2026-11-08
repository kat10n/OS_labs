CC = gcc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64
CFLAGS = -std=c11 -Wall -Wextra -O2 -g

all: parent child

parent: parent.c utils.c utils.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -pthread -o $@ parent.c utils.c $(LDLIBS)

child: child.c utils.c utils.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -pthread -o $@ child.c utils.c $(LDLIBS)

clean:
	rm -f parent child

.PHONY: all clean
