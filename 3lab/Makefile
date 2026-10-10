CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread

TARGET = msort
OBJS   = main.o msort.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c msort.h
	$(CC) $(CFLAGS) -pthread -c $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
