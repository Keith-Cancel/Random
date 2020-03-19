EXECUTABLE=nesys-enabler.exe

CC=gcc -O2 -Wall
LDFLAGS=-lgdi32 -lws2_32

src = $(wildcard *.c)
obj = $(src:.c=.o)

all: resource.o myprog

resource.o: resource.rc
	windres resource.rc. -O coff -o resource.o

myprog: $(obj)
	$(CC) -o $(EXECUTABLE) $^ $(LDFLAGS) resource.o -mwindows
	strip $(EXECUTABLE)

.PHONY: clean
clean:
	rm $(obj) $(EXECUTABLE) resource.o