# Non windows only!
# Requires Wine and mingw-w64!

CC = i686-w64-mingw32-gcc

LDFLAGS = -ldxguid -ldplayx -lole32 -luuid

SOURCES = $(shell find . -name "*.c")
HEADERS = $(shell find . -name "*.h")
OBJECTS = $(SOURCES:.c=.o)

all: bin/debug/dprun.exe bin/release/dprun.exe
	@echo "Sizes:"
	@ls -sh1 bin/*/dprun.exe
debug: bin/debug/dprun.exe
release: bin/release/dprun.exe

clean:
	rm -f $(OBJECTS)
	rm -f bin/debug/dprun.exe bin/release/dprun.exe

.PHONY: all debug release clean

%.o: %.c $(HEADERS)
	$(CC) -c -Wall -g -I./include $< -o $@

bin/debug:
	mkdir -p bin/debug
bin/release:
	mkdir -p bin/release

bin/debug/dprun.exe: bin/debug $(OBJECTS)
	$(CC) -Wall -DWIN32_LEAN_AND_MEAN -g -o $@ $(OBJECTS) -static $(LDFLAGS)

bin/release/dprun.exe: bin/release $(OBJECTS)
	$(CC) -Wall -DWIN32_LEAN_AND_MEAN -O3 -s -o $@ $(OBJECTS) -static $(LDFLAGS)

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
