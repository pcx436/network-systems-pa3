//
// Created by jmalcy on 11/17/20.
//

#ifndef HTTPPROXY_CACHE_H
#define HTTPPROXY_CACHE_H

#include <time.h>
#include <pthread.h>
#include <stdio.h>

typedef struct {
	time_t t;
	char *requestHash;
} cacheEntry;

struct cache {
	cacheEntry **array;
	pthread_mutex_t *mutex;
	pthread_mutex_t *hostnameMutex;
	char *cacheDirectory;
	char *dnsFile;
	int count;
	int capacity;
	int timeout;
};

struct cache *initCache(int timeout);

void addToCache(char *requestPath, const char *response, struct cache *cache);

FILE * cacheLookup(char *requestHash, struct cache *cache);

void deleteCacheEntry(char *requestHash, struct cache *cache);

void clearCache(struct cache *cache);

void freeCacheEntry(cacheEntry *cEntry);

#endif //HTTPPROXY_CACHE_H
