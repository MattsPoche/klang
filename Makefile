TARGET=kc
CFLAGS=-std=c23 -Wall -Wextra -Wswitch-enum -ggdb -Werror=vla -D_GNU_SOURCE
CFLAGS += -fwrapv -fno-strict-aliasing
HFILES=$(shell find -type f -name '*.h')
CFILES=$(shell find -type f -name '*.c')
OFILES=$(CFILES:%.c=%.o)
OPT=0
DEBUG=1

.PHONEY: test unity

$(TARGET): $(OFILES)
	$(CC) $(OFILES) -o $@

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -DKC_DEBUG=$(DEBUG) -O$(OPT) -c -o $@ $<

unity:
	$(CC) $(CFLAGS) -DKC_DEBUG=$(DEBUG) -DUNITY_BUILD -O$(OPT) -o $(TARGET) main.c

test: $(TARGET)
	./run_tests.py
