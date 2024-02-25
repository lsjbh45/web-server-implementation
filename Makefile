CC=gcc
CFLAGS=-g -Wall
OBJS=server.o
TARGET=server

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

server.o: server.c

clean:
	rm -f *.o
	rm -f $(TARGET)