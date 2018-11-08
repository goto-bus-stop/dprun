# Non windows only!
# Requires Wine and mingw-w64!

CC = i686-w64-mingw32-gcc

CFLAGS = -Wall -DWIN32_LEAN_AND_MEAN
LDFLAGS = -ldplayx -lole32

SOURCES = $(shell find . -name "*.c")
HEADERS = $(shell find . -name "*.h")
OBJECTS = $(SOURCES:.c=.o)
DLL_OBJECTS = dpsp.o dll.o

all: bin/debug/dprun.exe bin/release/dprun.exe
	@echo "Sizes:"
	@ls -sh1 bin/*/dprun.exe
debug: bin/debug/dprun.exe
release: bin/release/dprun.exe

clean:
	rm -f $(OBJECTS)
	rm -f *.plist
	rm -f bin/debug/dprun.exe bin/release/dprun.exe

.PHONY: all debug release clean

%.o: %.c $(HEADERS)
	$(CC) -c -Wall -g -I./include $< -o $@

bin/debug:
	mkdir -p bin/debug
bin/release:
	mkdir -p bin/release

bin/debug/dprun.dll: bin/debug $(DLL_OBJECTS)
	$(CC) $(CFLAGS) -DDEBUG -g -shared -o $@ $(DLL_OBJECTS) $(LDFLAGS)
bin/debug/dprun.exe: bin/debug/dprun.dll $(OBJECTS)
	$(CC) $(CFLAGS) -DDEBUG -g -o $@ $(OBJECTS) $(LDFLAGS)

bin/release/dprun.dll: bin/release $(DLL_OBJECTS)
	$(CC) $(CFLAGS) -O3 -s -shared -o $@ $(DLL_OBJECTS) $(LDFLAGS)
bin/release/dprun.exe: bin/release/dprun.dll $(OBJECTS)
	$(CC) $(CFLAGS) -O3 -s -o $@ $(OBJECTS) -static $(LDFLAGS)

lint:
	clang --analyze $(SOURCES) -I./include -I/usr/include/wine/windows/

run: bin/debug/dprun.exe
	wine bin/debug/dprun.exe

test-host: bin/debug/dprun.exe
	wine bin/debug/dprun.exe --host \
	  -p 'Host username' \
	  --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}' \
	  --service-provider '{36E95EE0-8577-11cf-960C-0080C7534E82}' \
	  --address INet=127.0.0.1

test-join: bin/debug/dprun.exe
	false

lines:
	wc -l *.h $(SOURCES)
