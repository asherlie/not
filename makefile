CC=gcc
CFLAGS=-Wall -Wextra -Werror -Wpedantic -pthread -lm -g

not: not.c peercalc.c sub_net.c node.c shared.c

.PHONY:
clean:
	rm not
