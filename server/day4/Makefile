CC = gcc
CFLAGS = -pthread

TARGETS = server

all: $(TARGETS)

server: hw4_server.c
	$(CC) $(CFLAGS) -o server hw4_server.c

ser: hw4_client.c
	$(CC) $(CFLAGS) -o client hw4_client.c

clean:
	rm -f $(TARGETS)

