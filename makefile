CFLAGS=-Wall -Wextra -Wswitch-enum -ggdb -Werror=vla
SOURCE=main.c
TARGET=kc

.depend: $(SOURCE)
	rm -f $@
	$(CC) $(CFLAGS) -MM $^ -MF $@

kc: main.o
	$(CC) $(LFLAGS) -o $@ $^

include .depend
