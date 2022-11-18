CC = gcc
FLAGS = -c -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude
OUT = bucketheap

all:
	$(CC) src/bucketheap.c -o src/bucketheap.o $(FLAGS)
	$(CC) src/abstraction.c -o src/abstraction.o $(FLAGS)
	ar rcs $(OUT).a src/bucketheap.o src/abstraction.o

debug:
	$(CC) src/bucketheap.c -g -o src/bucketheap.o $(FLAGS)
	$(CC) src/abstraction.c -g -o src/abstraction.o $(FLAGS)
	ar rcs $(OUT).a src/bucketheap.o src/abstraction.o

clean:
	rm src/bucketheap.o
	rm src/abstraction.o
	rm $(OUT).a

.PHONY: clean