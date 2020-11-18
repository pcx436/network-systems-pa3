//
// Created by jmalcy on 11/17/20.
//

#include "cache.h"
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

void clearCache(struct cache *cache) {

}
