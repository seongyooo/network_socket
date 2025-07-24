CC = gcc
CFLAGS = -pthread

TARGETS = client

all: $(TARGETS)

client: multi_game_client.c
	$(CC) $(CFLAGS) -o client multi_game_client.c

clean:
	rm -f $(TARGETS)

