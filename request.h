//
// Created by jmalcy on 11/16/20.
//

#ifndef HTTPPROXY_REQUEST_H
#define HTTPPROXY_REQUEST_H

#include "macro.h"
#include <semaphore.h>
#include <sys/time.h>

typedef struct request {
	char *method;  // should always be GET
	char *requestPath;
	char *protocol;
	char *host;
	char *originalBuffer;
	char *postProcessBuffer;
	int port;
} request;

typedef struct cacheEntry  {
	char *requestURL;
	char *response;
	struct timeval t;
} cacheEntry;

typedef struct threadParams {
	cacheEntry *cache;
	int *connfd;
	int *cacheSize;
	int *numThreads;
	pthread_mutex_t *cacheMutex;
	pthread_mutex_t *threadMutex;
} threadParams;

void readRequest(int connfd, request *req);

void parseRequest(request *req);

void trimSpace(char *s);


#endif //HTTPPROXY_REQUEST_H
