all: client server

client: client.o
	gcc client.c utils/client_utils.c -o client -lpthread -Wall

server: server.o
	gcc server.c utils/server_utils.c -o server -lpthread -Wall

clean:
	rm *o client server