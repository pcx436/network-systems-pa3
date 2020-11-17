//
// Created by jmalcy on 11/16/20.
//

#include <strings.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include "request.h"

const std::string &request::getMethod() const {
	return method;
}

void request::setMethod(const std::string &method) {
	request::method = method;
}

const std::string &request::getRequestPath() const {
	return requestPath;
}

void request::setRequestPath(const std::string &requestPath) {
	request::requestPath = requestPath;
}

const std::string &request::getAProtocol() const {
	return protocol;
}

void request::setAProtocol(const std::string &aProtocol) {
	protocol = aProtocol;
}

const std::string &request::getHost() const {
	return host;
}

void request::setHost(const std::string &host) {
	request::host = host;
}

int request::getPort() const {
	return port;
}

void request::setPort(int port) {
	request::port = port;
}

request::request() {}

void request::readRequest(int connfd) {
	int currentMax = MAXBUF;
	char *recvBuffer = new char [currentMax];
	buffer = new char [currentMax];

	bzero(buffer, currentMax);  // fill receiveBuffer with \0
	bzero(recvBuffer, currentMax);
	int numReceived = 0, currentReadNum;

	do {
		// read in some bytes from the user
		currentReadNum = recv(connfd, recvBuffer, MAXBUF, 0);

		if (currentReadNum > 0) {
			numReceived += currentReadNum;

			if (numReceived > MAXBUF) {
				currentMax *= 2;
				buffer = (char *) realloc(buffer, currentMax);

				if (buffer == nullptr) {
					std::cerr << "Failed to allocate buffer to meet client demands." << std::endl;
					exit(1);
				}
			}

			strcat(buffer, recvBuffer);
		} else if (currentReadNum < 0) {
			std::cerr << "Error reading data from user." << std::endl;
			exit(1);
		}
	} while (strstr(buffer, "\r\n\r\n") == nullptr && currentReadNum > 0);  // not done reading
	delete[] recvBuffer;
}

void request::parseRequest() {
	std::string s = std::string(buffer);
	std::stringstream ss(s);

	while(getline())
}

request::~request() {
	delete[] buffer;
}
