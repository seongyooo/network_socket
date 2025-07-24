CC = gcc
CFLAGS = -pthread

TARGETS = client

all: $(TARGETS)

client: hw4_client.c
	$(CC) $(CFLAGS) -o client hw4_client.c

clean:
	rm -f $(TARGETS)
