/**
 * server.c - A concurrent TCP webserver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>

#include "macro.h"
#include "request.h"

static volatile int killed = 0;

int open_listenfd(int port);

void respond(int connfd);

void *thread(void *vargp);

void trimSpace(char *str);

void interruptHandler(int useless) {
	killed = 1;
}


int main(int argc, char **argv) {
	int *connfdp;
	int listenfd, port, index, threadCount = 0, threadArraySize = MAX_THREADS, cacheSize = 0;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	struct threadParams *tps;
	pthread_mutex_t cacheMutex, threadMutex;
	pthread_t *threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * threadArraySize);
	cacheEntry *cache;

	// init shared content
	pthread_mutex_init(&cacheMutex, NULL);
	pthread_mutex_init(&threadMutex, NULL);
	cache = (cacheEntry *)malloc(sizeof(cacheEntry) * MAX_CACHE_ENTRIES);

	// register signal handler
	signal(SIGINT, interruptHandler);

	// check for incorrect usage
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}
	port = atoi(argv[1]);

	if (port < 1 || port > 65535) {
		perror("Invalid port number provided");
		return 1;
	}

	// create the socket we'll use
	if((listenfd = open_listenfd(port)) < 0) {
		perror("Could not open socket");
		return 1;
	}

	// while SIGINT not received
	while (!killed) {
		connfdp = (int *)malloc(sizeof(int));

		// If received connection, spawn thread. Else, free allocated memory
		if ((*connfdp = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen)) > 0){
			tps = (threadParams *)malloc(sizeof(threadParams));
			tps->cacheMutex = &cacheMutex;
			tps->threadMutex = &threadMutex;
			tps->cache = cache;
			tps->cacheSize = &cacheSize;
			tps->connfd = connfdp;
			tps->numThreads = &threadCount;

			if (threadCount == threadArraySize) {
				threadArraySize *= 2;
				threadIDs = (pthread_t *)realloc(threadIDs, sizeof(pthread_t) * threadArraySize);
			}

			pthread_create(&threadIDs[threadCount++], NULL, thread, (void *)tps);
		}
		else
			free(connfdp);
	}

	// join all threads
	for(index = 0; index < threadCount; index++)
		pthread_join(threadIDs[index], NULL);
	free(threadIDs);

	// clear the cache
	for (index = 0; index < cacheSize; index++) {
		free(cache[index].requestURL);
		free(cache[index].response);
	}
	free(cache);

	pthread_mutex_destroy(&cacheMutex);
	pthread_mutex_destroy(&threadMutex);
	return 0;
}

/* thread routine */
void *thread(void *vargp) {
	threadParams *tps = (threadParams *)vargp;
	int connfd = *tps->connfd;  // get the connection file descriptor
	free(tps->connfd);  // don't need that anymore since it was just an int anyway

	respond(connfd);  // run main thread function
	free(vargp);

	close(connfd);  // close the socket
	pthread_mutex_lock(tps->threadMutex);  // no idea if arithmetic operations are thread safe
	*tps->numThreads--;
	pthread_mutex_unlock(tps->threadMutex);
	return NULL;
}

/**
 * Responds to any HTTP requests.
 * @param connfd  connection file descriptor that represents the response socket
 */
void respond(int connfd) {
	char errorMessage[] = "%s 400 Bad Request\r\n";
	char receiveBuffer[MAXLINE], *response = (char *)malloc(MAXBUF);
	char *method, *uri, *version, *savePtr, *trashCan;

	bzero(receiveBuffer, MAXLINE);  // fill receiveBuffer with \0
	bzero(response, MAXBUF);  // fill response with \0

	// read in some bytes from the user
	read(connfd, receiveBuffer, MAXLINE);

	// logic time!
	// get HTTP request method, request URI, and request version separately


	// If the user provided the GET method, request URI, and HTTP version
	if (method && uri && version && strcmp(method, "GET") == 0) {  // happy path
		// get absoluteURI of request and ensure it is not trying to escape
	} else { // Invalid HTTP request, assume version 1.1
		sprintf(response, errorMessage, "HTTP/1.1");
	}

	if (strlen(response) > 0){
		send(connfd, response, strlen(response), 0);
	}

	// free remaining variables
	free(response);
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) {
	int listenfd, optval = 1, flags;
	struct sockaddr_in serveraddr;

	/* Create a socket descriptor */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	/* Eliminates "Address already in use" error from bind. */
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
	               (const void *) &optval, sizeof(int)) < 0)
		return -1;

	/* listenfd will be an endpoint for all requests to port
	   on any IP address for this host */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short) port);
	if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0)
	{
		return -1;
	}
	if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return -1;
	}

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, LISTENQ) < 0)
		return -1;
	return listenfd;
} /* end open_listenfd */

