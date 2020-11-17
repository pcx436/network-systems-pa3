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
	char *recvBuffer = malloc(sizeof(char) * currentMax), *buffer = req->originalBuffer;
	buffer = malloc(sizeof(char) * currentMax);

	bzero(buffer, currentMax);  // fill receiveBuffer with \0
	bzero(recvBuffer, currentMax);
	int numReceived = 0, currentReadNum;

	do {
		// read in some bytes from the user
		bzero(recvBuffer, MAXBUF);
		currentReadNum = recv(connfd, recvBuffer, MAXBUF, 0);

		if (currentReadNum > 0) {
			numReceived += currentReadNum;

			if (numReceived > MAXBUF) {
				currentMax *= 2;
				buffer = (char *) realloc(buffer, sizeof(char) * currentMax);

				if (buffer == NULL) {
					perror("Failed to allocate buffer to meet client demands.");
					exit(1);
				}
			}

			strcat(buffer, recvBuffer);
		} else if (currentReadNum < 0) {
			perror("Error reading data from user.");
			exit(1);
		}
	} while (strstr(buffer, "\r\n\r\n") == NULL && currentReadNum > 0);  // not done reading
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
			}

			req->host = tmp;
		} else {
			perror("Error parsing request: invalid host");
			exit(1);
		}
	}
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
