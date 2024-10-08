CC := cc
CFLAGS := -Wall -Wextra -pedantic

memori: memori.c
	$(CC) memori.c -o memori $(CFLAGS)