#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h> // sigaction(), sigsuspend(), sig*()
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "debug.h"
#include "server.h"
#include "directory.h"
#include "thread_counter.h"
#include "protocol.h"


static void terminate(int sig);
void *thread(void *vargp);
int open_listenfd(int port);




THREAD_COUNTER *thread_counter;


int main(int argc, char* argv[]) {
	//Parse arguments
	int c = 0;
	int port = -1;
	char* hostname = NULL;
	int qFlag = 0;
	while((c = getopt(argc, argv, "p:q:h:")) != -1){
		if(c == 'p'){
			sscanf(optarg, "%d", &port);
		}
		if(c == 'h'){
			hostname = (char*) malloc(sizeof(char) * strlen(optarg));
			strcpy(hostname, optarg);
		}
		if(c == 'q'){
			qFlag = 1;
		}
	}
	debug("Port: %d\n", port);
	debug("hostname: %s\n", hostname);
	debug("qFlag: %d\n", qFlag);


	//Set up SIGHUP Handler
	struct sigaction sigHandler;
	sigHandler.sa_handler = &terminate;
	sigHandler.sa_flags = SA_RESTART;
	sigfillset(&sigHandler.sa_mask);
	if(sigaction(SIGHUP, &sigHandler, NULL) == -1) {
		perror("Error: cannot handle SIGHUP");
	}



	// Perform required initializations of the thread counter and directory.
	thread_counter = tcnt_init();
	dir_init();


	socklen_t clientlen = sizeof(struct sockaddr);
	struct sockaddr_in clientaddr;
	pthread_t tid; 

	int *connfdp;

	int listenfd = open_listenfd(port);
	while(1){
		connfdp = (int*)malloc(sizeof(int));
		*connfdp = accept(listenfd, ((struct sockaddr*) (&clientaddr)), &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);
	}

	fprintf(stderr, "You have to finish implementing main() "
		"before the Bavarde server will function.\n");

	terminate(0);
	return 1;
}


int open_listenfd(int port){

	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;

	//Create Socket Descriptor
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*) &optval, sizeof(int)) < 0)
		return -1;

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);

	if (bind(listenfd, ((struct sockaddr*) (&serveraddr)), sizeof(serveraddr)) < 0)
		return -1;

	if (listen(listenfd, 1023) < 0)
		return -1;
	return listenfd;
}



void *thread(void *vargp){
	int connfd = *((int*)vargp);
	bvd_client_service(vargp);
	debug("connfd: %d\n", connfd);
	pthread_detach(pthread_self());
	//free(vargp);
	close(connfd);
	return NULL;
}


/*
 * Function called to cleanly shut down the server.
 */
void terminate(int sig) {
	// Shut down the directory.
	// This will trigger the eventual termination of service threads.
	dir_shutdown();
	
	debug("Waiting for service threads to terminate...");
	tcnt_wait_for_zero(thread_counter);
	debug("All service threads terminated.");

	tcnt_fini(thread_counter);
	dir_fini();
	exit(EXIT_SUCCESS);
}
