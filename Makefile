CC = gcc
CFLAGS = -g -Wall -Wextra -pthread -fsanitize=thread
TARGETS = simple_test thread_test

all: $(TARGETS)

%: %.c mallocule.h
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./simple_test
	./thread_test

clean:
	rm -f $(TARGETS)

.PHONY: all clean run
