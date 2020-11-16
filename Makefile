CC=gcc
CFLAGS=-I. -Wall -Werror -g 

default: server

server: server.o
	$(CC) -o server server.o -lpthread

clean:
	rm server.o
	rm server
