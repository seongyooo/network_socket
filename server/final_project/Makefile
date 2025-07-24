CC = gcc
CFLAGS = -pthread

TARGETS = server

all: $(TARGETS)

server: multi_game_server.c
	$(CC) $(CFLAGS) -o server multi_game_server.c

clean:
	rm -f $(TARGETS)

