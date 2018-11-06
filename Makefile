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

test-host: dprun.exe
	wine dprun.exe --host \
	  -p 'Host username' \
	  --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}' \
	  --service-provider '{36E95EE0-8577-11cf-960C-0080C7534E82}'

test-join: dprun.exe
	false
