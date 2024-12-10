CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = xsh
SRC = xsh_shell_program.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
