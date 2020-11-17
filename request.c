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
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "request.h"

void readRequest(int connfd, request *req) {
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
					exit(1);
				}
			}

			strcat(req->originalBuffer, recvBuffer);
		} else if (currentReadNum < 0) {
			perror("Error reading data from user.");
			exit(1);
		}
	} while (strstr(req->originalBuffer, "\r\n\r\n") == NULL && currentReadNum > 0);  // not done reading
	free(recvBuffer);
}

void parseRequest(request *req) {
	char *tmp = NULL, *savePtr = NULL, *finder = NULL;
	req->postProcessBuffer = (char *)malloc(strlen(req->originalBuffer));

	bzero(req->postProcessBuffer, strlen(req->originalBuffer));

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
			exit(1);
		}
	}
}

cacheEntry *forwardRequest(request *req) {
	int sock, bytesCopied = 0, bytesSent, serverlen, totalReceived = 0, bytesReceived, entrySize = MAXBUF;
	cacheEntry *cEntry = (cacheEntry *)malloc(sizeof(cacheEntry));
	struct sockaddr_in server;
	struct hostent *hostLookup;
	char *socketBuffer = (char *)malloc(sizeof(char) * MAXBUF);
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Couldn't open socket to destination");
		return NULL;
	}

	server.sin_addr.s_addr = inet_addr("0.0.0.0");
	server.sin_family = AF_INET;
	server.sin_port = htons(req->port);  // pick random t

	hostLookup = gethostbyname(req->host);
	if (hostLookup == NULL) { // FIXME: need to respond properly to unknown host
		perror("Could not find hostname of specified host");
		return NULL;
	}
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Failed to connect to destination");
		return NULL;
	}

	// allocate for the cache
	cEntry->requestURL = (char *)malloc(strlen(req->requestPath));
	cEntry->response = (char *)malloc(sizeof(char) * MAXBUF);
	strcpy(cEntry->requestURL, req->requestPath);

	// forward request
	do {
		strcpy(socketBuffer, req->originalBuffer + bytesCopied);

		bytesSent = send(sock, socketBuffer, MAXBUF, 0);
		if (bytesSent < 0) {
			perror("Error sending data");
		} else {
			bytesCopied += bytesSent;  // Should this be bytesCopied += MAXBUF??
		}
	} while (strstr(socketBuffer, "\r\n\r\n") == NULL && bytesSent >= 0);

	// receive response
	do {
		bzero(socketBuffer, MAXBUF);
		bytesReceived = recv(sock, socketBuffer, MAXBUF, 0);

		if (bytesReceived < 0) {
			perror("Error reading response");
		} else {
			totalReceived += bytesReceived;

			while (entrySize < totalReceived) {
				entrySize *= 2;

				cEntry->response = (char *)realloc(cEntry->response, sizeof(char) * entrySize);
				if (cEntry->response == NULL) {
					perror("Failed growing memory for new cache entry");
					bytesReceived = MAXBUF - 1;  // breaks loop
				}
			}

			strcat(cEntry->response, socketBuffer);
		}

	} while (bytesReceived == MAXBUF);
	close(sock);
	free(socketBuffer);

	return cEntry;
}

void sendResponse(int connfd, cacheEntry *cEntry) {
	char *socketBuffer = (char *)malloc(sizeof(char) * MAXBUF);
	int bytesCopied = 0, bytesSent;

	do {
		strcpy(socketBuffer, cEntry->response + bytesCopied);

		bytesSent = send(connfd, socketBuffer, MAXBUF, 0);

		if (bytesSent < 0) {
			perror("Error sending data back to client");
			bytesCopied = strlen(cEntry->response);  // break loop
		} else {
			bytesCopied += bytesSent;  // Should this be bytesCopied += MAXBUF??
		}
	} while (bytesCopied < strlen(cEntry->response));
	free(socketBuffer);
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
