CC = gcc
SA = scan-build
OUT = mcbot
_FILES = bot.c marshal.c protocol.c
FDIR = src
FILES= $(patsubst %,$(FDIR)/%,$(_FILES))
CFLAGS=-Wall --std=c99

build:
	$(CC) -o $(OUT) $(FILES) $(CFLAGS)

check:
	$(SA) make

clean:
	rm -f *.o core

rebuild: clean build
