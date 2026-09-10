#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/* there are 32-bit and 64-bit versions of this DLL in 64-bit Windows */
#pragma comment(lib, "Ws2_32.lib")

#include "lxScribo.h"
#include "NXP_I2C.h"

static SOCKET clientSocket = INVALID_SOCKET;

static void lxScriboWinsockExit(void)
{
	printf("%s closing sockets\n", __FUNCTION__);

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

static int lxScriboWinsockInit(char *server)
{
	WSADATA wsaData;
	int result;
	char *hostname, *portnr;
	struct sockaddr_in sin;
	struct hostent *host;
	//int port;
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

	portnr = strrchr(server, ':');
	if (portnr == NULL) {
		fprintf (stderr, "%s: %s is not a valid servername, use host:port\n",__FUNCTION__, server);
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

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		fprintf(stderr, "socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	memcpy(&sin.sin_addr.s_addr, host->h_addr, host->h_length);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if (connect(clientSocket, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr,"error connecting to %s:%d\n", hostname , port);
		closesocket(clientSocket);
		WSACleanup();
		return -1;
	}

	if (init_done == 0) {
		atexit(lxScriboWinsockExit);
	}

	return (int)clientSocket;
}

const struct nxp_i2c_device lxScriboWinsock_device = {
	lxScriboWinsockInit,
	lxScriboWriteRead,
	lxScriboVersion,
	lxScriboSetPin,
	lxScriboGetPin,
        lxScriboClose
};

