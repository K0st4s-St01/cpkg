CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2 -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/json.c src/manifest.c src/build.c src/deps.c
OBJ = $(SRC:.c=.o)
TARGET = cpkg
PREFIX ?= /usr/local

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
