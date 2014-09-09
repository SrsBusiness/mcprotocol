CC = gcc
SA = scan-build
OUT = mcbot
OUTTEST = test_protocol
_FILES = marshal.c protocol.c
_MAIN = bot.c
_TESTMAIN = test_protocol.c
FDIR = src
FILES= $(patsubst %,$(FDIR)/%,$(_FILES))
MAIN= $(patsubst %,$(FDIR)/%,$(_MAIN))
TESTMAIN= $(patsubst %,$(FDIR)/%,$(_TESTMAIN))
CFLAGS=-Wall --std=gnu99

build:
	$(CC) -o $(OUT) $(FILES) $(MAIN) $(CFLAGS)

test_protocol:
	$(CC) -o $(OUTTEST) $(FILES) $(TESTMAIN) $(CFLAGS)

check:
	$(SA) make build

clean:
	rm -f *.o core

rebuild: clean build
