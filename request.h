//
// Created by jmalcy on 11/16/20.
//

#ifndef HTTPPROXY_REQUEST_H
#define HTTPPROXY_REQUEST_H

#include "macro.h"
#include <semaphore.h>
#include <sys/time.h>

typedef struct {
	char *method;  // should always be GET
	char *requestPath;
	char *protocol;
	char *host;
	char *originalBuffer;
	char *postProcessBuffer;
	int port;
} request;

typedef struct {
	struct timeval t;
	char *requestURL;
	char *response;
} cacheEntry;

typedef struct {
	int *connfd;
	int *cacheSize;
	int *numThreads;
	pthread_mutex_t *cacheMutex;
	pthread_mutex_t *threadMutex;
	cacheEntry *cache;
} threadParams;

void readRequest(int connfd, request *req);

void parseRequest(request *req);

cacheEntry *forwardRequest(request *req);

void sendResponse(int connfd, cacheEntry *cEntry);

void trimSpace(char *s);


#endif //HTTPPROXY_REQUEST_H
