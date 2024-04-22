all: main

CC = clang
override CFLAGS += -g -Wno-everything -pthread -lm
INCLUDES = -I.

SRCS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.c' -print)
HEADERS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.h' -print)

main: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o "$@"

main-debug: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -O0 $(SRCS) -o "$@"

clean:
	rm -f main main-debug