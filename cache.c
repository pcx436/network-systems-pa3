//
// Created by jmalcy on 11/17/20.
//

#include "cache.h"
#include "md5.h"
#include "macro.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <linux/limits.h>

struct cache *initCache(int timeout) {
	// temp dir initialization
	int initialCacheCapacity = 1;
	const char *dirStr = "/tmp/proxyCache.XXXXXX", *cacheFileName = "/dnsCache.csv";

	char *tmpDir = NULL, *tmpTemplate = malloc(strlen(dirStr) + 1), *hostnameTemplate = NULL;

	pthread_mutex_t *mutex, *hostnameMutex;
	cacheEntry **array;
	struct cache *newCache;

	// Memory allocation check failures
	if (tmpTemplate == NULL) {
		perror("Failed to allocate memory for temporary directory name");
		return NULL;
	}

	hostnameTemplate = malloc(strlen(dirStr) + strlen(cacheFileName) + 1);
	if (hostnameTemplate == NULL) {
		perror("hostname template did a bad, stop the wizard!!!");
		free(tmpTemplate);
		return NULL;
	}

	strcpy(tmpTemplate, dirStr);

	if ((tmpDir = mkdtemp(tmpTemplate)) == NULL) {
		perror("Failed to create cache directory");
		free(tmpTemplate);
		free(hostnameTemplate);
		return NULL;
	}

	strcpy(hostnameTemplate, tmpDir);
	strcat(hostnameTemplate, cacheFileName);

	// memory allocation for mutex
	if ((mutex = malloc(sizeof(pthread_mutex_t))) == NULL) {
		perror("Failed to allocate memory for cache mutex");
		free(tmpTemplate);
		free(hostnameTemplate);
		return NULL;
	}
	pthread_mutex_init(mutex, NULL);

	if ((hostnameMutex = malloc(sizeof(pthread_mutex_t))) == NULL) {
		perror("Failed to allocate memory for cache mutex");
		free(tmpTemplate);
		free(hostnameTemplate);
		pthread_mutex_destroy(mutex);
		free(mutex);
		return NULL;
	}
	pthread_mutex_init(hostnameMutex, NULL);

	// cache array allocation
	if ((array = malloc(sizeof(cacheEntry *) * initialCacheCapacity)) == NULL) {
		perror("Failed to allocate in-memory cache");
		free(tmpTemplate);
		free(hostnameTemplate);
		pthread_mutex_destroy(mutex);
		pthread_mutex_destroy(hostnameMutex);
		free(mutex);
		free(hostnameMutex);

		return NULL;
	}

	// cache struct allocation
	if ((newCache = malloc(sizeof(struct cache))) == NULL) {
		perror("Failed to allocate cache struct");
		free(tmpTemplate);
		free(hostnameTemplate);
		pthread_mutex_destroy(mutex);
		pthread_mutex_destroy(hostnameMutex);
		free(mutex);
		free(hostnameMutex);
		free(array);

		return NULL;
	}

	/**
	 * Finally! If we get to this point then all of the memory allocations passed.
	 * They will probaby always passed on a non-embedded system, but it's still nice to have the checks in place.
	 * Now we build the actual struct that we're going to return.
	 */
	 newCache->array = array;
	 newCache->mutex = mutex;
	 newCache->hostnameMutex = hostnameMutex;
	 newCache->dnsFile = hostnameTemplate;
	 newCache->cacheDirectory = tmpDir;
	 newCache->count = 0;
	 newCache->capacity = initialCacheCapacity;
	 newCache->timeout = timeout;

	fprintf(stderr, "Cache diretory is %s\n", tmpDir);
	return newCache;
}

void addToCache(char *requestHash, struct cache *cache) {
	// error check
	if (requestHash == NULL || cache == NULL)
		return;

	// first, check to see if entry is already in the cache
	FILE *lookupResult;

	// try doing cache lookup on md5 result
	pthread_mutex_lock(cache->mutex);

	// File is already in the cache, ignore and leave
	if ((lookupResult = cacheLookup(requestHash, cache, LOCK_DISABLED)) != NULL) {
		fclose(lookupResult);
		pthread_mutex_unlock(cache->mutex);
		return;
	}

	// second: Didn't find file in cache, time to add it.
	cacheEntry *cEntry;
	if ((cEntry = malloc(sizeof(cacheEntry))) == NULL) {
		perror("Failed cacheEntry malloc during addToCache");

		pthread_mutex_unlock(cache->mutex);
		return;
	}
	cEntry->requestHash = requestHash;
	cEntry->t = time(NULL);

	// do we have to double cache size to fit new entry?
	if (cache->count == cache->capacity) {
		cache->capacity *= 2;

		cache->array = (cacheEntry **)realloc(cache->array, sizeof(cacheEntry *) * cache->capacity);
	}

	// Add element to cache, increase the count
	cache->array[cache->count++] = cEntry;

	pthread_mutex_unlock(cache->mutex);
	pthread_t id;
	struct destructionArgs *dArgs = malloc(sizeof(struct destructionArgs));
	dArgs->cEntry = cEntry;
	dArgs->c = cache;
	dArgs->doDetach = 1;
	pthread_create(&id, NULL, deleteCacheEntry, (void *)dArgs);
}

FILE *cacheLookup(char *requestHash, struct cache *cache, int lockEnabled) {
	// error check
	if (requestHash == NULL || cache == NULL)
		return NULL;

	int i;
	FILE *returnValue = NULL;
	char fileName[PATH_MAX];  // one for / one for \0

	if (lockEnabled == LOCK_ENABLED)
		pthread_mutex_lock(cache->mutex);

	// First, check if requestHash in cache. Second, open cache file if possible.
	for(i = 0; i < cache->count && returnValue == NULL && !errno; i++) {
		if (strcmp(requestHash, cache->array[i]->requestHash) == 0) {  // found in cache, try to open file
			sprintf(fileName, "%s/%s", cache->cacheDirectory, requestHash);
			returnValue = fopen(fileName, "r");
		}
	}

	if (lockEnabled == LOCK_ENABLED)
		pthread_mutex_unlock(cache->mutex);

	return returnValue;
}

void * deleteCacheEntry(void *dArgs) {
	int i, foundIndex = -1;
	struct destructionArgs *destArgs = (struct destructionArgs *)dArgs;
	struct cache *cache = destArgs->c;
	cacheEntry *cEntry = destArgs->cEntry;

	if (destArgs -> doDetach == 1)
		pthread_detach(pthread_self());

	free(destArgs);
	sleep(cache->timeout);
	pthread_mutex_lock(cache->mutex);

	// get index of cache entry.
	for(i = 0; i < cache->count && foundIndex == -1; i++) {
		if (strcmp(cEntry->requestHash, cache->array[i]->requestHash) == 0) {
			// found cache element!
			foundIndex = i;
		}
	}

	// didn't find it.
	if (foundIndex == -1) {
		pthread_mutex_unlock(cache->mutex);
		return NULL;
	}

	// shift cache
	for (i = foundIndex; i < cache->count - 1; i++)
		cache->array[i] = cache->array[i + 1];

	// removed entry
	cache->array[cache->count--] = NULL;

	pthread_mutex_unlock(cache->mutex);

	char fullName[PATH_MAX];
	bzero(fullName, PATH_MAX);
	snprintf(fullName, PATH_MAX, "%s/%s", cache->cacheDirectory, cEntry->requestHash);

	freeCacheEntry(cEntry);

	remove(fullName);
	return NULL;
}

// also acts as a destructor for the cache
void clearCache(struct cache *cache) {
	// no need to use mutex lock since this should only be called during termination of the main
	int i;
	struct destructionArgs *dArgs;
	for (i = 0; i < cache->count; i++) {
		dArgs = malloc(sizeof(struct destructionArgs));
		dArgs->c = cache;
		dArgs->cEntry = cache->array[i];
		dArgs->doDetach = 0;
		deleteCacheEntry((void *)dArgs);
	}

	// TODO: Delete cache directory
	pthread_mutex_destroy(cache->mutex);
	pthread_mutex_destroy(cache->hostnameMutex);
	free(cache->array);
	free(cache->mutex);
	free(cache->hostnameMutex);
	free(cache->cacheDirectory);
	free(cache->dnsFile);
	free(cache);
}

void freeCacheEntry(cacheEntry *cEntry) {
	free(cEntry->requestHash);
	free(cEntry);
}
