all: client server

client: client.o
	gcc -Wall client.c utils/client_utils.c -o client

server: server.o
	gcc -Wall server.c utils/server_utils.c -o server

clean:
	rm *o client server