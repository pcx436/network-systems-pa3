//
// Created by jmalcy on 11/17/20.
//

#include "cache.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

char * cacheLookup(char *requestPath, struct cache *cache) {
	int i;
	char *returnValue = NULL;
	pthread_mutex_lock(cache->mutex);

	for(i = 0; i < *cache->size && returnValue == NULL; i++)
		if (strcmp(requestPath, cache->array[i]->requestURL) == 0)
			returnValue = cache->array[i]->response;

	pthread_mutex_unlock(cache->mutex);
	return returnValue;
}

void deleteCacheEntry(char *requestPath, struct cache *cache) {
	pthread_mutex_lock(cache->mutex);
	int i, foundIndex = -1;
	cacheEntry *cEntry = NULL;

	// get index of cache entry.
	for(i = 0; i < *cache->size && foundIndex == -1; i++) {
		if (strcmp(requestPath, cache->array[i]->requestURL) == 0) {
			foundIndex = i;
			cEntry = cache->array[i];
		}
	}

	// didn't find it.
	if (foundIndex == -1)
		return;

	// shift cache
	for (i = foundIndex; i < *cache->size - 1; i++)
		cache->array[i] = cache->array[i + 1];

	// removed entry
	*cache->size--;

	freeCacheEntry(cEntry);
	pthread_mutex_unlock(cache->mutex);
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
