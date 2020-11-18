//
// Created by jmalcy on 11/17/20.
//

#ifndef HTTPPROXY_CACHE_H
#define HTTPPROXY_CACHE_H

typedef struct {
	time_t t;
	char *requestURL;
	char *response;
} cacheEntry;

#endif //HTTPPROXY_CACHE_H
