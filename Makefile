CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2

SRC := linked-malloc.c
TARGET := linked-malloc.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -shared -fPIC -fvisibility=hidden -o $(TARGET) $(SRC)

clean:
	rm $(TARGET)
