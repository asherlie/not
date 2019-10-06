CC=gcc
CFLAGS=-Wall -Wextra -Werror -Wpedantic -pthread -lm -g

not: not.c peercalc.c

.PHONY:
clean:
	rm not
