TARGET=kc
CFLAGS=-std=c23 -Wall -Wextra -Wswitch-enum -ggdb -Werror=vla -D_GNU_SOURCE
CFLAGS += -fwrapv -fno-strict-aliasing
#CFLAGS += -fsanitize=address,undefined
#CFLAGS=-Wall -Wextra -Wswitch-enum -ggdb -Werror=vla
DEPS=$(shell find -type f -name '*.[ch]')
OPT=0
DEBUG=1

.PHONEY: test

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) -DKC_DEBUG=$(DEBUG) -O$(OPT) -o $(TARGET) main.c

test: $(TARGET)
	./run_tests.py
