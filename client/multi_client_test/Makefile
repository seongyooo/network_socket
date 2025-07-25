CC = gcc
CFLAGS = -pthread

TARGETS = client

all: $(TARGETS)

client: multi_test.c
	$(CC) $(CFLAGS) -o client multi_test.c

clean:
	rm -f $(TARGETS)

