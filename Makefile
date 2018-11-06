CC = i686-w64-mingw32-gcc

LDFLAGS = -ldxguid -ldplayx -lole32 -luuid

SOURCES = $(shell find . -name "*.c")
OBJECTS = $(SOURCES:.c=.o)

%.o: %.c
	$(CC) -c -Wall -g -I./include $< -o $@

dprun.exe: $(OBJECTS)
	$(CC) -Wall -g -o $@ $(OBJECTS) -static $(LDFLAGS)

all: dprun.exe

run: all
	wine dprun.exe
