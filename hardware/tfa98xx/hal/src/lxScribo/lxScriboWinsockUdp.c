#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/* there are 32-bit and 64-bit versions of this DLL in 64-bit Windows */
#pragma comment(lib, "Ws2_32.lib")

#include "lxScribo.h"
#include "NXP_I2C.h"

static SOCKET clientSocket = INVALID_SOCKET;
static struct sockaddr_in client_addr; /* remote address */

static void lxScriboWinsockExit(void)
{
	//printf("%s closing sockets\n", __FUNCTION__);

	if (clientSocket != INVALID_SOCKET) {
		shutdown(clientSocket, SD_BOTH);
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}
	/*
	 * workaround: sleep for a while, give Windows some time to cleanup
	 * socket might already have been closed by Ws2_32.dll,
	 * then the shutdown/closesocket will fail with WSANOTINITIALISED
	 */
	if (WSAGetLastError() == WSANOTINITIALISED) {
		Sleep(100);
	}

	WSACleanup();
}

static int lxScriboWinsockInitUDP(char *server)
{
	WSADATA wsaData;
	int result;
	char *hostname, *portnr;
	struct hostent *host;
	unsigned short port;
	int init_done = 0;

	if (server == 0) {
		fprintf (stderr, "%s:called for recovery, exiting for now...\n", __FUNCTION__);
		return -1;
	}

	/* Initialize Winsock */
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (result != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", result);
		return -1;
	}

	/* lxScribo register can be called multiple times */
	init_done = (clientSocket != INVALID_SOCKET);

	if (init_done) {
		fprintf(stderr, "%s: closing already open socket\n", __FUNCTION__);
		closesocket(clientSocket);
	}

	portnr = strrchr(server, '@');
	if (portnr == NULL) {
		fprintf (stderr, "%s: %s is not a valid servername, use host@port\n",__FUNCTION__, server);
		return -1;
	}
	hostname=server;
	*portnr++ ='\0'; //terminate
	port=(unsigned short)atoi(portnr);
	host = gethostbyname(hostname);
	if (!host) {
		fprintf(stderr, "Error: wrong hostname: %s\n", hostname);
		return -1;
	}

	if (port==0) // illegal input
		return -1;

	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		fprintf(stderr, "socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	memcpy(&client_addr.sin_addr.s_addr, host->h_addr, host->h_length);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(port);

	if (init_done == 0) {
		atexit(lxScriboWinsockExit);
	}

	return (int)clientSocket;
}

/*
 * Winsock variant of udp_read()
 */
int winsock_udp_read(int fd, char *inbuf, int len, int waitsec) {
	socklen_t addrlen = sizeof(client_addr); /* length of addresses */
	int recvlen; /* # bytes received */
	DWORD dwTime = (waitsec*1000); // timeout value in miliseconds

	if (waitsec) {
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&dwTime, sizeof(dwTime)) < 0) {
			perror("Error");
		}
	}

	recvlen = recvfrom(fd, inbuf, len, 0, (struct sockaddr *) &client_addr, &addrlen);
	if (recvlen >= 0) {
	//	inbuf[recvlen] = 0;
	//	printf("received message: \"%s\" (%d bytes)\n", inbuf, recvlen);
	} else
		perror("rcv error");

	return recvlen;
}

/*
 * Winsock variant of udp_write()
 */
int winsock_udp_write(int fd, char *outbuf, int len){
	return sendto(fd, outbuf, len, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
}

static int lxScriboUdpClose(int fd)
{
	(void)fd; /* Remove unreferenced parameter warning C4100 */
	fprintf(stderr, "Function close not implemented for target ScriboUdp.");
	return 0;
}

const struct nxp_i2c_device lxScriboWinsock_udp_device = {
	lxScriboWinsockInitUDP,
	lxScriboUdpWriteRead,
	lxScriboUdpVersion,
	lxScriboUdpSetPin,
	lxScriboUdpGetPin,
	lxScriboUdpClose
};

