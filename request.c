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
#include "request.h"
#include "cache.h"
#include "md5.h"

char * readRequest(int connfd, request *req) {
	int currentMax = MAXBUF;
	char *recvBuffer = malloc(sizeof(char) * MAXBUF);
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
					free(recvBuffer);
					return NULL;
				}
			}

			strcat(req->originalBuffer, recvBuffer);
		} else if (currentReadNum < 0) {
			perror("Error reading data from user.");
			free(recvBuffer);
			return NULL;
		}
	} while (currentReadNum > 0);  // not done reading
	free(recvBuffer);

	return req->originalBuffer;
}

char * parseRequest(request *req) {
	char *tmp = NULL, *savePtr = NULL, *finder = NULL;
	req->postProcessBuffer = (char *)malloc(strlen(req->originalBuffer) + 1);

	strcpy(req->postProcessBuffer, req->originalBuffer);

	// grab the method stuff
	req->method = strtok_r(req->postProcessBuffer, " ", &savePtr);
	req->requestPath = strtok_r(NULL, " ", &savePtr);
	req->protocol = strtok_r(NULL, "\n", &savePtr);
	trimSpace(req->protocol);

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
	struct sockaddr_in server;
	char socketBuffer[MAXBUF], fileName[PATH_MAX];
	FILE *returnFile = NULL;
	struct hostent *hostLookup = NULL;

	// open returnFile
	bzero(fileName, PATH_MAX);
	snprintf(fileName, PATH_MAX, "%s/%s", cache->cacheDirectory, req->requestHash);

	if ((returnFile = fopen(fileName, "w+")) == NULL) {
		perror("failed opening new cache file");
		return NULL;
	}

	// check the hostname file
	hostLookup = gethostbyname(req->host);
	if (hostLookup == NULL) { // FIXME: need to respond properly to unknown host
		perror("Could not find hostname of specified host");
		fclose(returnFile);
		return NULL;
	}

	// open socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Couldn't open socket to destination");
		fclose(returnFile);
		return NULL;
	}

	// set socket args
	server.sin_addr.s_addr = inet_addr(req->host);
	server.sin_family = AF_INET;
	server.sin_port = htons(req->port);  // pick random t

	// connect socket
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Failed to connect to destination");
		fclose(returnFile);
		close(sock);
		return NULL;
	}

	// forward request
	do {
		bzero(socketBuffer, MAXBUF);
		strcpy(socketBuffer, req->originalBuffer + bytesCopied);

		bytesSent = send(sock, socketBuffer, strlen(socketBuffer), 0);
		if (bytesSent < 0) {
			perror("Error sending data");
		} else {
			bytesCopied += bytesSent;  // Should this be bytesCopied += MAXBUF??
		}
	} while (bytesSent > 0);  // TODO: check this

	// receive response
	do {
		bzero(socketBuffer, MAXBUF);
		bytesReceived = recv(sock, socketBuffer, MAXBUF, 0);

		if (bytesReceived < 0) {
			perror("Error reading response");
		} else {
			totalReceived += bytesReceived;
			fwrite(socketBuffer, sizeof(char), bytesReceived, returnFile);
		}

	} while (bytesReceived == MAXBUF);
	close(sock);
	fseek(returnFile, 0, SEEK_SET);

	addToCache(req->requestHash, cache);
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

	char *savePoint = NULL, *domain = "\0", *ip, *returnIP = NULL;
	char lineBuf[MAXLINE];
	FILE *dnsFile = NULL;
	int lookupError;
	struct addrinfo hints, *infoResults;
	struct hostent *hostLookup = NULL;

	if ((returnIP = malloc(INET6_ADDRSTRLEN)) == NULL) {
		perror("Failed to allocate IPv4 cache read buffer");
		return NULL;
	}
	bzero(returnIP, INET6_ADDRSTRLEN);

	// hints setup
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	pthread_mutex_lock(cache->hostnameMutex);
	// TODO: Implement blacklist check
	if ((dnsFile = fopen(cache->dnsFile, "r")) != NULL) {  // found cache file
		while (strcmp(domain, hostname) != 0 && fgets(lineBuf, MAXLINE, dnsFile)) {
			domain = strtok_r(lineBuf, ",", &savePoint);
			ip = strtok_r(NULL, "\n", &savePoint);
		}
		fclose(dnsFile);

		// found IP address in cache file
		if (domain != NULL && ip != NULL && strcmp(domain, hostname) == 0 && strcmp(ip, "UNKNOWN") != 0) {
			strcpy(returnIP, ip);
		} else if (domain != NULL && strcmp(domain, hostname) != 0) {
			// not found in cache. Will have to add it now.
			hostLookup = gethostbyname(hostname);

			// resolved hostname
			if (hostLookup != NULL && hostLookup->h_length > 0) {
				strcpy(returnIP, hostLookup->h_addr_list[0]);
			}
		}

	} else {  // couldn't open the cache file, just try resolving
		hostLookup = gethostbyname(hostname);

		// resolved hostname
		if (hostLookup != NULL && hostLookup->h_length > 0) {
			strcpy(returnIP, hostLookup->h_addr_list[0]);
		}
	}

	// write to cache if we didn't grab the IP from cache
	if (domain == NULL && (dnsFile = fopen(cache->dnsFile, "a")) != NULL){
		if (strlen(returnIP) == 0)
			fprintf(dnsFile, "%s,UNKNOWN\n", hostname);
		else
			fprintf(dnsFile, "%s,%s\n", hostname, returnIP);
		fclose(dnsFile);
	}
	pthread_mutex_unlock(cache->hostnameMutex);

	// final return system
	return returnIP;
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
