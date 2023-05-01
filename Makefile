CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2

SRC := linked-malloc.c
TARGET := linked-malloc.so

all: linked-malloc.so

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -shared -fPIC -o $(TARGET) $(SRC)

clean:
	rm linked-malloc.so
