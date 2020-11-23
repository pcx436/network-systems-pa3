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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>

#include "macro.h"
#include "request.h"
#include "md5.h"

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
	int listenfd, port, index, threadCount = 0, threadArraySize = 1, cacheTimeout = 60;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	struct cache *cache;
	threadParams *tps;
	pthread_mutex_t threadMutex;
	pthread_t *threadIDs;

	// init shared content
	pthread_mutex_init(&threadMutex, NULL);

	// register signal handler
	signal(SIGINT, interruptHandler);

	// check for incorrect usage
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s <port> [timeout]\n", argv[0]);
		exit(0);
	} else {
		port = atoi(argv[1]);
		if (argc == 3) {
			cacheTimeout = atoi(argv[2]);

			if (cacheTimeout <= 0) {
				perror("Invalid cache timeout. Must be greater than 0");
				return 1;
			}
		}
	}

	if (port < 1 || port > 65535) {
		perror("Invalid port number provided");
		return 1;
	}

	if ((cache = initCache(cacheTimeout)) == NULL) {
		perror("Failed cache initialization");
		return 1;
	}

	if ((threadIDs = malloc(sizeof(pthread_t) * threadArraySize)) == NULL) {
		perror("Failed to allocate thread arrays");
		clearCache(cache);
		return 1;
	}
	// create the socket we'll use
	if((listenfd = open_listenfd(port)) < 0) {
		perror("Could not open socket");
		free(threadIDs);
		clearCache(cache);
		return 1;
	}

	// while SIGINT not received
	while (!killed) {
		connfdp = (int *)malloc(sizeof(int));

		// If received connection, spawn thread. Else, free allocated memory
		if ((*connfdp = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen)) > 0){
			tps = (threadParams *)malloc(sizeof(threadParams));
			tps->cache = cache;
			tps->numThreads = &threadCount;
			tps->threadMutex = &threadMutex;
			tps->connfd = connfdp;

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
	for (index = 0; index < threadCount; index++)
		pthread_join(threadIDs[index], NULL);
	free(threadIDs);

	pthread_mutex_destroy(&threadMutex);
	return 0;
}

/* thread routine */
void *thread(void *vargp) {
	threadParams *tps = (threadParams *)vargp;
	FILE *serverResponse = NULL;
	char errorMessage[] = "%s 400 Bad Request\r\n";
	int connfd = *tps->connfd;  // get the connection file descriptor
	free(tps->connfd);  // don't need that anymore since it was just an int anyway

	request *req = malloc(sizeof(request));
	readRequest(connfd, req);  // receive data from client
	parseRequest(req);  // parse data from client into readable format

	// check if in cache
	if ((serverResponse = cacheLookup(req->requestHash, tps->cache, LOCK_ENABLED)) == NULL) {  // cache lookup failed
		serverResponse = forwardRequest(req, tps->cache);
	}

	if (serverResponse != NULL)
		sendResponse(connfd, serverResponse);

	close(connfd);  // close the socket
	pthread_mutex_lock(tps->threadMutex);  // no idea if arithmetic operations are thread safe
	*tps->numThreads--;
	pthread_mutex_unlock(tps->threadMutex);
	free(tps);
	return NULL;
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

