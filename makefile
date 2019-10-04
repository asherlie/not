CC=gcc
CFLAGS=-Wall -Wextra -Werror -Wpedantic -lpthread -g

not: not.c

.PHONY:
clean:
	rm not
