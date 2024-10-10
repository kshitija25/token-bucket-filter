CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -pthread
LDFLAGS = -pthread -lm

TARGET  = token_bucket_filter.exe
OBJS    = TrafficShaper.o list.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

TrafficShaper.o: TrafficShaper.c list.h util.h
	$(CC) $(CFLAGS) -c TrafficShaper.c

list.o: list.c list.h util.h
	$(CC) $(CFLAGS) -c list.c

clean:
	rm -f *.o $(TARGET)
