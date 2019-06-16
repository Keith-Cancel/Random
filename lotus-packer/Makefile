CDEBUG  =
C_OPT   = -O3
CFLAGS  = -Wall -W -Wextra -std=c99 -trigraphs -pedantic $(CDEBUG) $(C_OPT)
CC      = gcc
BIN_EXT =

# For gcc MinGW compiler when using Msys2 for instance
ifeq ($(OS), Windows_NT)
BIN_EXT :=.exe
endif

all: packer$(BIN_EXT)

file-list.o: file-list.c file-list.h
	$(CC) $(CFLAGS) -c $< -o $@

packer.o: packer.c file-list.h
	$(CC) $(CFLAGS) -c $< -o $@

packer$(BIN_EXT): packer.o file-list.o
	$(CC) $^ -o $@

clean:
	rm -f packer$(BIN_EXT)
	rm -f *.o
