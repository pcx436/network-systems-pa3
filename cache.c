//
// Created by jmalcy on 11/17/20.
//

#include "cache.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

char * cacheLookup(char *requestPath, struct cache *cache) {
	int i;
	pthread_mutex_lock(cache->mutex);

	for(i = 0; i < *cache->size; i++)
		if (strcmp(requestPath, cache->array[i]->requestURL) == 0)
			return cache->array[i]->response;

	pthread_mutex_unlock(cache->mutex);
	return NULL;
}

void deleteCacheEntry(char *requestPath, struct cache *cache) {

}

// also acts as a destructor for the cache
void clearCache(struct cache *cache) {
	// no need to use mutex lock since this should only be called during termination of the main
	int i;
	for (i = 0; i < *cache->size; i++)
		freeCacheEntry(NULL);

	free(cache->array);
	pthread_mutex_destroy(cache->mutex);
	free(cache);
}

void freeCacheEntry(cacheEntry *cEntry) {
	free(cEntry->requestURL);
	free(cEntry->response);
	free(cEntry);
}
