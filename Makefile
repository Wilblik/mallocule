CC = gcc
CFLAGS = -g -Wall -Wextra
TARGET = test_mallocule

all: $(TARGET)

$(TARGET): test.c mallocule.h
	$(CC) $(CFLAGS) -o $(TARGET) test.c

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean run
