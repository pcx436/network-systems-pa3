//
// Created by jmalcy on 11/17/20.
//

#ifndef HTTPPROXY_CACHE_H
#define HTTPPROXY_CACHE_H

#include <time.h>
#include <pthread.h>

typedef struct {
	time_t t;
	char *requestURL;
	char *response;
} cacheEntry;

struct cache {
	cacheEntry **array;
	pthread_mutex_t *mutex;
	int *size;
	int timeout;
};

char * cacheLookup(char *requestPath, struct cache *cache);

void deleteCacheEntry(char *requestPath, struct cache *cache);

void clearCache(struct cache *cache);

#endif //HTTPPROXY_CACHE_H
