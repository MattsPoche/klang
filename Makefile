TARGET=kc
CFLAGS=-Wall -Wextra -Wswitch-enum -ggdb -Werror=vla
DEPS=$(shell find -type f -name '*.[ch]')

.PHONEY: test

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) -o $(TARGET) main.c

test: $(TARGET)
	./run_tests.py
