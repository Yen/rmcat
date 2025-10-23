CC = cc
CFLAGS = -std=c99 -O2 -Wall -Wextra -pedantic
TARGET = rmcat
SRC = main.c
PREFIX = /usr/local

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

.PHONY: clean install uninstall

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)