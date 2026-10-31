CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Wpedantic -O2 -D_XOPEN_SOURCE=700
SRC=src/main.c src/backup.c src/logger.c
OBJ=$(SRC:.c=.o)
TARGET=backup

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -I./src -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f logs/backup.log

.PHONY: all clean
