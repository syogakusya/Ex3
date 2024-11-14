CC = cc
CLIENT = client.out
SERVER = server.out

.PHONY: all build run clean

build: $(CLIENT) $(SERVER)

$(CLIENT): client.c
	$(CC) -o $(CLIENT) client.c

$(SERVER): server.c
	$(CC) -o $(SERVER) server.c

run: build
	./$(SERVER) &
	./$(CLIENT)

all: build run

clean:
	rm -f $(CLIENT) $(SERVER)
