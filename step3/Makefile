CC := gcc
CFLAGS := -Wall -g -std=c11 -Wno-unused-parameter -I./fs/include
BIN_DIR := build

.PHONY: all clean

all: $(BIN_DIR)/disk $(BIN_DIR)/fs $(BIN_DIR)/client

$(BIN_DIR)/disk: disk/disk.c
	$(CC) -o $@ $<

$(BIN_DIR)/fs: fs/src/*.c
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/client: client/client.c
	$(CC) -o $@ $<

clean:
	rm -f $(BIN_DIR)/disk $(BIN_DIR)/fs $(BIN_DIR)/client
