/**
 * This file implements a "UDP ping server".
 *
 * It basically waits for datagrams to arrive, and for each one received, it responds to the original sender
 * with another datagram that has the same payload as the original datagram. The server must reply to 3
 * datagrams and then quit.
 *
 * Test with:
 *
 * $ netcat localhost 8080
 * ping
 * ping
 * pong
 * pong
 *
 * (assuming that this program listens at localhost port 8080)
 *
 */

// TODO: some includes here

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "logger.h"

int main(int argc, char **argv) {

	(void) argc; // This is how an unused parameter warning is silenced.
	(void) argv;

	// TODO: some socket stuff here

	int sockfd;
	char buffer[1024];
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
        	perror("Error en la creacion del socket");
        	exit(EXIT_FAILURE);
    	}

	serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(8080);

	if(bind(sockfd,(const struct sockaddr*)&serveraddr, sizeof(serveraddr))< 0){
        	perror("Error en el bind");
        	exit(EXIT_FAILURE);
    	}
	
	int x;
    	int l = sizeof(clientaddr);
	
	for(int i=0; i<3; i++){
		x = recvfrom(sockfd, (char*)buffer, 1024, MSG_WAITALL, (struct sockaddr*) &clientaddr, &l);
		log_message(LOG_INFO, buffer);
		sendto(sockfd, (const char*)buffer, x, MSG_CONFIRM, (const struct sockaddr*) &clientaddr, l);
	}
	
	close(sockfd);
	return 0;
}
