CC = cc
CLIENT = client
SERVER = server

.PHONY: all build server client clean

all: clean build

build:
	$(CC) -o $(CLIENT).out $(CLIENT).c
	$(CC) -o $(SERVER).out $(SERVER).c

server:
	./$(SERVER).out

client:
	./$(CLIENT).out

clean:
	rm -f $(CLIENT).out $(SERVER).out
