CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -pthread -Iinclude
LDFLAGS = -pthread

SERVER  = bin/server
DATA    = data

# Исходники сервера
SERVER_SRC = src/server.c \
             src/logger.c \
             src/storage.c \
             src/session.c \
             src/http_server.c

.PHONY: all clean run dirs

all: dirs $(SERVER)

dirs:
	@mkdir -p bin $(DATA)

$(SERVER): $(SERVER_SRC) | dirs
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)

run: $(SERVER)
	./$(SERVER)

clean:
	rm -rf bin