CC = gcc
CFLAGS = -Wall 

TARGETS = server

all: $(TARGETS)

server: hw3_server.c
	$(CC) $(CFLAGS) -o server hw3_server.c

ser: hw3_client.c
	$(CC) $(CFLAGS) -o client hw3_client.c

clean:
	rm -f $(TARGETS)

