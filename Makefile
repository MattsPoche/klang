TARGET    =  kc
CFLAGS    =  -std=c23 -Wall -Wextra -Wswitch-enum -ggdb -Werror=vla -D_GNU_SOURCE -fwrapv -fno-strict-aliasing
LFLAGS    =
FSANITIZE =  -fsanitize=address -fsanitize=undefined
HFILES=$(shell find -type f -name '*.h')
CFILES=$(shell find -type f -name '*.c')
OFILES=$(CFILES:%.c=%.o)
OPT=0
DEBUG=1

.PHONEY: test clean

$(TARGET): $(OFILES)
	$(CC) $(LFLAGS) $(FSANITIZE) $(OFILES) -o $@

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) $(FSANITIZE) -DKC_DEBUG=$(DEBUG) -O$(OPT) -c -o $@ $<

test: $(TARGET)
	./run_tests.py

clean:
	rm -f $(shell find -type f -name '*.o') $(TARGET) a.out
