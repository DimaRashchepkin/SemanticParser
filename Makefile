CC = gcc
CFLAGS = -Wall -Wextra -std=c11

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET = app

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
