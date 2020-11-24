CC=gcc
CFLAGS=-I. -Wall -g

default: webproxy

webproxy: webproxy.c
	$(CC) -o webproxy md5.h macro.h request.h cache.h md5.c request.c cache.c webproxy.c -lpthread -lm

clean:
	rm *.o
	rm webproxy
