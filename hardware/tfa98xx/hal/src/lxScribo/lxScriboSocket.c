/*Simple Echo Server, demonstrating basic socket system calls.
 * Binds to random port between 9000 and 9999,
 * echos back to client all received messages
 * 'quit' message from client kills server gracefully
 *
 * To test server operation, open a telnet connection to host
 * E.g. telnet YOUR_IP PORT
 * PORT will be specified by server when run
 * Anything sent by telnet client to server will be echoed back
 *
 *
 * Written by: Ajay Gopinathan, Jan 08
 * ajay.gopinathan@ucalgary.ca
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

#include "lxScribo.h"
#include "NXP_I2C.h"

#define INVALID_SOCKET -1

static int listenSocket = INVALID_SOCKET;
static int activeSocket = INVALID_SOCKET;

typedef void (*sighandler_t)(int);

/*
 *
 */
void lxScriboSocketExit(int status)
{
	char buf[256];
	struct linger l;

	l.l_onoff = 1;
	l.l_linger = 0;

	printf("%s closing sockets\n", __FUNCTION__);

	// still bind error after re-open when traffic was active
	if (listenSocket>0) {
		shutdown(listenSocket, SHUT_RDWR);
		//close(listenSocket);
	}
	if (activeSocket>0) {
		setsockopt(activeSocket, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
		shutdown(activeSocket, SHUT_RDWR);
		read(activeSocket, buf, 256);
		close(activeSocket);
	}

	activeSocket = INVALID_SOCKET;
	listenSocket = INVALID_SOCKET;

	_exit(status);
}

/*
 * ctl-c handler
 */
static void lxScriboCtlc(int sig)
{
    (void)sig;
	(void)signal(SIGINT, SIG_DFL);
	lxScriboSocketExit(0);
}

/*
 * exit handler
 */
static void lxScriboAtexit(void)
{
	lxScriboSocketExit(0);
}

int lxScriboSocketInit(char *server)
{
	char *hostname, *portnr;
	struct sockaddr_in sin;
	struct hostent *host;
	int port;
	int init_done = 0;

	if ( server==0 ) {
		fprintf (stderr, "%s:called for recovery, exiting for now...", __FUNCTION__);
		lxScriboSocketExit(1);
	}

	/* lxScribo register can be called multiple times */
	init_done = (activeSocket != INVALID_SOCKET);
	if (init_done) {
		fprintf(stderr, "%s: closing already open socket\n", __FUNCTION__);
		shutdown(activeSocket, SHUT_RDWR);
		close(activeSocket);
		usleep(10000);
	}

	portnr = strrchr(server , ':');
	if ( portnr == NULL )
	{
		fprintf (stderr, "%s: %s is not a valid servername, use host:port\n",__FUNCTION__, server);
		return -1;
	}
	hostname=server;
	*portnr++ ='\0'; //terminate
	port=atoi(portnr);

	host = gethostbyname(hostname);
	if ( !host ) {
		fprintf(stderr, "Error: wrong hostname: %s\n", hostname);
		exit(1);
	}

	if(port==0) // illegal input
		return -1;

	activeSocket = socket(AF_INET, SOCK_STREAM, 0);  /* init socket descriptor */

	/*** PLACE DATA IN sockaddr_in struct ***/
	memcpy(&sin.sin_addr.s_addr, host->h_addr, host->h_length);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	/*** CONNECT SOCKET TO THE SERVICE DESCRIBED BY sockaddr_in struct ***/
	if (connect(activeSocket, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr,"error connecting to %s:%d\n", hostname , port);
		return -1;
	}

	if (init_done == 0) {
		atexit(lxScriboAtexit);
		(void) signal(SIGINT, lxScriboCtlc);
	}

	return activeSocket;
}

/*
 * the sockets are created first and then waits until a connection is done
 * the active socket is returned
 */
int lxScriboListenSocketInit(char *socketnr)
{
	int port;
	int  rc;
	char hostname[50];
	char clientIP [INET6_ADDRSTRLEN];

	port = atoi(socketnr);
	if(port==0) // illegal input
		return -1;

	rc = gethostname(hostname,sizeof(hostname));

	if(rc == -1){
		printf("Error gethostname\n");
		return -1;
	}

	struct sockaddr_in serverAdd;
	struct sockaddr_in clientAdd;
	socklen_t clientAddLen;

	atexit(lxScriboAtexit);
	(void) signal(SIGINT, lxScriboCtlc);

	printf("Listening to %s:%d\n", hostname, port);

	memset(&serverAdd, 0, sizeof(serverAdd));
	serverAdd.sin_family = AF_INET;
	serverAdd.sin_port = htons(port);

	//Bind to any local server address using htonl (host to network long):
	serverAdd.sin_addr.s_addr = htonl(INADDR_ANY);
	//Or specify address using inet_pton:
	//inet_pton(AF_INET, "127.0.0.1", &serverAdd.sin_addr.s_addr);

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(listenSocket == -1){
		printf("Error creating socket\n");
		return -1;
	}

	if(bind(listenSocket, (struct sockaddr*) &serverAdd, sizeof(serverAdd)) == -1){
		printf("Bind error\n");
		return -1;
	}

	if(listen(listenSocket, 5) == -1){
		printf("Listen Error\n");
		return -1;
	}

	clientAddLen = sizeof(clientAdd);
	activeSocket = accept(listenSocket, (struct sockaddr*) &clientAdd, &clientAddLen);

	inet_ntop(AF_INET, &clientAdd.sin_addr.s_addr, clientIP, sizeof(clientAdd));
	printf("Received connection from %s\n", clientIP);

	close(listenSocket);

	return (activeSocket);

}

const struct nxp_i2c_device lxScriboSocket_device = {
		lxScriboSocketInit,
		lxScriboWriteRead,
		lxScriboVersion,
		lxScriboSetPin,
		lxScriboGetPin,
		lxScriboClose
};

