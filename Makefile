# Non windows only!
# Requires Wine and mingw-w64!

CC = i686-w64-mingw32-gcc

CFLAGS = -Wall
LDFLAGS = -ldplayx -lole32 -lws2_32 -luuid

OPTFLAGS = -O3 -s
DBGFLAGS = -DDEBUG -g

SOURCES = $(shell echo *.c cli/*.c)
HEADERS = $(shell echo *.h cli/*.h)
OBJECTS = $(SOURCES:.c=.o)
DLL_SOURCES = $(shell echo dll/*.c)
DLL_HEADERS = $(shell echo dll/*.h)
DLL_OBJECTS = $(DLL_SOURCES:.c=.o) debug.o

all: bin/debug/dprun.exe bin/release/dprun.exe
	@echo "Sizes:"
	@ls -sh1 bin/debug/dprun.exe bin/debug/dprun.dll \
			   bin/release/dprun.exe bin/release/dprun.dll
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
	$(CC) -o $@ $(CFLAGS) $(DBGFLAGS) -shared $(DLL_OBJECTS) $(LDFLAGS)
bin/debug/dprun.exe: bin/debug/dprun.dll $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(DBGFLAGS) $(OBJECTS) $(LDFLAGS)

bin/release/dprun.dll: bin/release $(DLL_OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OPTFLAGS) -shared $(DLL_OBJECTS) $(LDFLAGS)
bin/release/dprun.exe: bin/release/dprun.dll $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OPTFLAGS) -static $(OBJECTS) $(LDFLAGS)

lint:
	clang --analyze $(SOURCES) -I./include -I/usr/include/wine/windows/
	clang --analyze $(DLL_SOURCES) -I./include -I/usr/include/wine/windows/

run: bin/debug/dprun.exe
	wine bin/debug/dprun.exe

test-host: bin/debug/dprun.exe
	wine bin/debug/dprun.exe --host \
	  -p 'Host username' \
	  --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}' \
	  --service-provider TCPIP

test-join: bin/debug/dprun.exe dbg_sessid.txt
	wine bin/debug/dprun.exe --join `cat dbg_sessid.txt` \
	  -p 'Join username' \
	  --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}' \
	  --service-provider TCPIP \
	  --address INet=127.0.0.1

lines:
	wc -l *.h $(SOURCES)
