CC = gcc
OUT = mcbot
_FILES = bot.c marshal.c protocol.c
FDIR = src
FILES= $(patsubst %,$(FDIR)/%,$(_FILES))
CFLAGS=-Wall

build: 
	$(CC) -o $(OUT) $(FILES) $(CFLAGS)

clean:
	rm -f *.o core

rebuild: clean build
