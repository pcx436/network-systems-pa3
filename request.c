//
// Created by jmalcy on 11/16/20.
//

#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <netinet/in.h>
#include <linux/limits.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "request.h"
#include "cache.h"
#include "md5.h"

char * readRequest(int connfd, request *req) {
	int currentMax = MAXBUF;
	char recvBuffer[MAXBUF];
	req->originalBuffer = (char *)malloc(sizeof(char) * currentMax);

	bzero(req->originalBuffer, currentMax);  // fill receiveBuffer with \0
	bzero(recvBuffer, currentMax);
	int numReceived = 0, currentReadNum;

	do {
		// read in some bytes from the user
		bzero(recvBuffer, MAXBUF);
		currentReadNum = recv(connfd, recvBuffer, MAXBUF, 0);

		if (currentReadNum > 0) {
			numReceived += currentReadNum;

			while (numReceived > currentMax) {  // grow buffer to meet demand of newly read data
				currentMax *= 2;
				req->originalBuffer = (char *) realloc(req->originalBuffer, sizeof(char) * currentMax);

				if (req->originalBuffer == NULL) {
					perror("Failed to allocate buffer to meet client demands.");
					return NULL;
				}
			}

			strcat(req->originalBuffer, recvBuffer);
		} else if (currentReadNum < 0) {
			perror("Error reading data from user.");
			return NULL;
		}
	} while (currentReadNum == MAXBUF);  // not done reading

	return req->originalBuffer;
}

char *parseRequest(request *req, const char *cacheDir) {
	char *tmp = NULL, *savePtr = NULL, *finder = NULL;
	req->postProcessBuffer = (char *)malloc(strlen(req->originalBuffer) + 1);

	strcpy(req->postProcessBuffer, req->originalBuffer);

	// grab the method stuff
	req->method = strtok_r(req->postProcessBuffer, " ", &savePtr);
	req->requestPath = strtok_r(NULL, " ", &savePtr);
	req->protocol = strtok_r(NULL, "\n", &savePtr);
	trimSpace(req->protocol);

	if (strcmp(req->method, "GET") != 0) {
		free(req->postProcessBuffer);
		return NULL;
	}

	tmp = strtok_r(NULL, " ", &savePtr);
	if (tmp != NULL && strcmp(tmp, "Host:") == 0){  // host specification
		tmp = strtok_r(NULL, "\n", &savePtr);  // grab the rest of the line

		if (tmp != NULL) {
			trimSpace(tmp);
			finder = strchr(tmp, ':');

			if (finder != NULL) {  // port number found
				req->port = atoi(finder + 1);
				finder[0] = '\0';
			} else {
				req->port = 80;
			}

			req->host = tmp;
		} else {
			perror("Error parsing request: invalid host");
			free(req->postProcessBuffer);
			free(req->originalBuffer);
			return NULL;
		}
	}

	// MD5 hash requestPath
	if ((req->requestHash = malloc(HEX_BYTES + 1)) == NULL) {
		perror("Failed allocating requestHash in parseResponse");
		free(req->originalBuffer);
		free(req->postProcessBuffer);
		return NULL;
	}
	md5Str(req->requestPath, req->requestHash);

	return req->postProcessBuffer;
}

FILE * forwardRequest(request *req, struct cache *cache) {
	int sock, bytesCopied = 0, bytesSent, totalReceived = 0, bytesReceived;
	long contentLength = 0, headerSize = 0;
	char socketBuffer[MAXBUF], fileName[PATH_MAX], *lengthHeaderLocation, *eol, tmp, *endOfHeader;
	const char *headerStr = "Content-Length: ";
	FILE *returnFile = NULL;
	struct sockaddr_in *server;
	struct addrinfo *infoResults;

	// check the hostname file
	infoResults = hostnameLookup(req->host, cache);
	if (infoResults == NULL) {
		fprintf(stderr, "Could not find hostname of specified host: %s\n", req->host);
		return NULL;
	}

	char *blackListName = "/blacklist";
	char *bFN = malloc(strlen(cache->cacheDirectory) + strlen(blackListName) + 1);
	strcpy(bFN, cache->cacheDirectory);
	strcat(bFN, blackListName);
	FILE *blacklist = fopen(bFN, "r");
	free(bFN);

	int found = 0;
	if (blacklist != NULL) {
		char lineBuf[MAXLINE];
		char ip[INET6_ADDRSTRLEN];
		while (fgets(lineBuf, MAXLINE, blacklist) && !found) {
			trimSpace(lineBuf);
			if (isdigit(lineBuf[0])) {
				inet_ntop(infoResults->ai_family, &((struct sockaddr_in *)infoResults->ai_addr)->sin_addr,
						ip, INET6_ADDRSTRLEN);
				found = strcmp(lineBuf, ip);
			} else {
				found = strcmp(lineBuf, req->host) == 0;
			}
		}
		fclose(blacklist);
	}

	if (found == 1) {
		freeaddrinfo(infoResults);
		return (FILE *)1;
	}


	// open returnFile
	bzero(fileName, PATH_MAX);
	snprintf(fileName, PATH_MAX, "%s/%s", cache->cacheDirectory, req->requestHash);

	if ((returnFile = fopen(fileName, "w")) == NULL) {
		perror("failed opening new cache file");
		freeaddrinfo(infoResults);
		return NULL;
	}

	// open socket
	if ((sock = socket(infoResults->ai_family, SOCK_STREAM, 0)) < 0) {
		perror("Couldn't open socket to destination");
		fclose(returnFile);
		freeaddrinfo(infoResults);
		return NULL;
	}

	// set socket args
	server = (struct sockaddr_in *) infoResults->ai_addr;
	server->sin_port = htons(req->port);  // pick random t

	// connect socket
	if (connect(sock, (struct sockaddr *)server, infoResults->ai_addrlen) < 0) {
		fprintf(stderr, "Failed to connect to destination %s: %s\n", req->requestPath, strerror(errno));
		fclose(returnFile);
		freeaddrinfo(infoResults);
		close(sock);
		return NULL;
	}

	// forward request
	do {
		bzero(socketBuffer, MAXBUF);
		memcpy(socketBuffer, req->originalBuffer + bytesCopied, strlen(req->originalBuffer + bytesCopied));

		bytesSent = send(sock, socketBuffer, strlen(socketBuffer), 0);
		if (bytesSent < 0) {
			perror("Error sending data");
		} else {
			bytesCopied += bytesSent;  // Should this be bytesCopied += MAXBUF??
		}
	} while (bytesSent == MAXBUF);  // TODO: check this

	// receive response
	do {
		bzero(socketBuffer, MAXBUF);
		bytesReceived = recv(sock, socketBuffer, MAXBUF, 0);

		if (endOfHeader == NULL && (endOfHeader = strstr(socketBuffer, "\r\n\r\n")) != NULL) {
			tmp = endOfHeader[4];
			endOfHeader[4] = '\0';
			headerSize = strlen(socketBuffer);
			endOfHeader[4] = tmp;
		}
		// TODO: Deal with HTTP/1.1 transfer encoding chunked.
		if ((lengthHeaderLocation = strstr(socketBuffer, headerStr)) != NULL) {
			if ((eol = strstr(lengthHeaderLocation + strlen(headerStr), "\r\n")) != NULL) {
				eol[0] = '\0';  // finish line
				contentLength = atol(lengthHeaderLocation + strlen(headerStr));
				eol[0] = '\r';
			}
		}

		if (bytesReceived < 0) {
			perror("Error reading response");
		} else {
			totalReceived += bytesReceived;
			fwrite(socketBuffer, sizeof(char), bytesReceived, returnFile);
		}

	} while (bytesReceived == MAXBUF || totalReceived < (contentLength + headerSize));
	close(sock);
	fclose(returnFile);
	returnFile = fopen(fileName, "r");

	freeaddrinfo(infoResults);

	addToCache(req->requestHash, cache);
	printf("Added %s (%s) to cache\n", req->requestPath, req->requestHash);
	return returnFile;
}

void sendResponse(int connfd, FILE *cacheFile) {
	int bytesRead, bytesSent;
	char socketBuffer[MAXBUF];

	do {
		bzero(socketBuffer, MAXBUF);
		bytesRead = fread(socketBuffer, sizeof(char), MAXBUF, cacheFile);  // FIXME: conversion issue?
		bytesSent = send(connfd, socketBuffer, bytesRead, 0);

		if (bytesSent < 0) {
			perror("Error sending data back to client");
			bytesRead = MAXBUF - 1;  // break loop
		}
	} while (bytesRead == MAXBUF);
}

// FIXME: this function messed up
struct addrinfo * hostnameLookup(char *hostname, struct cache *cache) {
	if (cache == NULL || hostname == NULL)
		return NULL;

	char *savePoint = NULL, *domain = "\0", *ip;
	char lineBuf[MAXLINE], translateIP[INET6_ADDRSTRLEN];
	FILE *dnsFile = NULL;
	int lookupError = 1;  // all error codes are < 0, and since 0 is "success" we need something greater than 0
	struct addrinfo hints, *infoResults = NULL;

	// hints setup
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	pthread_mutex_lock(cache->hostnameMutex);
	// TODO: Implement blacklist check
	if ((dnsFile = fopen(cache->dnsFile, "r")) != NULL) {  // found cache file
		while (strcmp(domain, hostname) != 0 && fgets(lineBuf, MAXLINE, dnsFile)) {
			domain = strtok_r(lineBuf, ",", &savePoint);
			ip = strtok_r(NULL, "\n", &savePoint);
		}
		fclose(dnsFile);
		pthread_mutex_unlock(cache->hostnameMutex);

		// found IP address in cache file
		if (domain != NULL && ip != NULL && strcmp(domain, hostname) == 0 && strcmp(ip, "UNKNOWN") != 0) {
			hints.ai_flags |= AI_NUMERICHOST;  // found in cache, build socket

			lookupError = getaddrinfo(ip, NULL, &hints, &infoResults);
		} else if (domain != NULL && strcmp(domain, hostname) != 0) {
			// not found in cache. Will have to add it now.
			lookupError = getaddrinfo(hostname, NULL, &hints, &infoResults);
		}

	} else {  // couldn't open the cache file, just try resolving
		pthread_mutex_unlock(cache->hostnameMutex);
		lookupError = getaddrinfo(hostname, NULL, &hints, &infoResults);
	}

	// write to cache if we didn't grab the IP from cache
	pthread_mutex_lock(cache->hostnameMutex);
	dnsFile = fopen(cache->dnsFile, "a");

	if (dnsFile == NULL)
		perror("Failed to open DNS cache file for writing");
	else if (lookupError < 0 && strcmp(domain, hostname) != 0)  // failed lookup, didn't find it in the file
		fprintf(dnsFile, "%s,UNKNOWN\n", hostname);
	else if (lookupError == 0 && !(AI_NUMERICHOST & hints.ai_flags)){  // we have not seen this IP before
		inet_ntop(infoResults->ai_family, &((struct sockaddr_in *)infoResults->ai_addr)->sin_addr,
				translateIP, INET6_ADDRSTRLEN);
		fprintf(dnsFile, "%s,%s\n", hostname, translateIP);
	}
	fclose(dnsFile);
	pthread_mutex_unlock(cache->hostnameMutex);

	// final return system
	return infoResults;
}


/**
 * Removes trailing spaces from a string.
 * @param str The string to trim space from.
 */
void trimSpace(char *s){
	int end;
	if (s == NULL)
		return;

	end = strlen(s) - 1;
	while (isspace(s[end]))
		end--;
	s[end + 1] = '\0';
}
