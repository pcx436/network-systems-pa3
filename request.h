//
// Created by jmalcy on 11/16/20.
//

#ifndef HTTPPROXY_REQUEST_H
#define HTTPPROXY_REQUEST_H

#include "macro.h"
#include "cache.h"
#include <semaphore.h>
#include <time.h>

typedef struct {
	char *method;  // should always be GET
	char *requestPath;
	char *protocol;
	char *host;
	char *originalBuffer;
	char *postProcessBuffer;
	char *requestHash;
	int port;
} request;

typedef struct {
	int *connfd;
	int *numThreads;
	pthread_mutex_t *threadMutex;
	struct cache *cache;
} threadParams;

char * readRequest(int connfd, request *req);

char * parseRequest(request *req);

FILE * forwardRequest(request *req, struct cache *cache);

void sendResponse(int connfd, FILE *cacheFile);

struct addrinfo * hostnameLookup(char *hostname, struct cache *cache);

void trimSpace(char *s);

#endif //HTTPPROXY_REQUEST_H
