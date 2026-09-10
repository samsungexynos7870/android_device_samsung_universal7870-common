/*
 * udp_sockets.c
 *
 *  Created on: Apr 20, 2015
 *      Author: wim
 */

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#ifdef LXSCRIBO
#include "lxScribo.h"
#include "NXP_I2C.h"
#endif

#include "udp_sockets.h"

#define BUFSIZE 2048

static struct sockaddr_in myaddr;	/* our address */
static struct sockaddr_in remaddr;	/* remote address */

/*
    Get ip from domain name
 */

int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if ( (he = gethostbyname( hostname ) ) == NULL)
    {
        // get the host info
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    for(i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }

    return 1;
}

int udp_socket_init(char *server, int port)
{
	struct hostent *host;
	char ip[100];
	int fd;				/* our socket */

	if(port==0) // illegal input
		return -1;

	/* create an UDP socket */

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket\n");
		return 0;
	}

	/* bind the socket to any valid IP address and a specific port */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (server==0)  /* server only */
		myaddr.sin_port = htons(port);
	else /* client only */
		myaddr.sin_port = htons(0);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}

	if (server!=NULL) { /* client only */
		host = gethostbyname(server);
		if ( !host ) {
			fprintf(stderr, "Error: wrong hostname: %s\n", server);
			return -1;
		}

		/* now define remaddr, the address to whom we want to send messages */
		/* For convenience, the host address is expressed as a numeric IP address */
		/* that we will convert to a binary format via inet_aton */
		memset((char *) &remaddr, 0, sizeof(remaddr));
		memcpy(&remaddr.sin_addr.s_addr, host->h_addr, host->h_length);
		remaddr.sin_family = AF_INET;
		remaddr.sin_port = htons(port);

		/* get ip dot name */
		if (hostname_to_ip(server , ip))
			return -1;
		/* convert char dot ip to binary form */
		if (inet_aton(ip, &remaddr.sin_addr)==0) {
			fprintf(stderr, "inet_aton() failed\n");
			return -1;
		}
	}
	return fd;
}

int udp_read(int fd, char *inbuf, int len, int waitsec) {
	socklen_t addrlen = sizeof(remaddr); /* length of addresses */
	int recvlen; /* # bytes received */
	struct timeval tv;
	tv.tv_sec = waitsec;
	tv.tv_usec = 0;

	if (waitsec) {
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
			perror("Error");
		}
	}

	recvlen = recvfrom(fd, inbuf, len, 0, (struct sockaddr *) &remaddr,
			&addrlen);
	if (recvlen >= 0) {
	//	inbuf[recvlen] = 0;
	//	printf("received message: \"%s\" (%d bytes)\n", inbuf, recvlen);
	} else
		perror("rcv error");

	return recvlen;
}

int udp_write(int fd, char *outbuf, int len){
	return sendto(fd, outbuf, len, 0, (struct sockaddr *)&remaddr, sizeof(remaddr));
}

/**
 * below is the test main for client and server executables to do standalone testing
 */
#if 0 // avoid multiple definition of 'main'
//#ifndef LXSCRIBO
#ifdef SERVER
int main() {
	int fd = udp_socket_init(NULL, 5555);
	int recvlen;
	unsigned char buf[BUFSIZE];

	for(;;) {
		printf("waiting on port %d\n", 5555);
		recvlen = udp_read(fd, buf, sizeof(buf), 0);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			printf("received message: \"%s\" (%d bytes)\n", buf, recvlen);
		}
		else
			printf("uh oh - something went wrong!\n");

		strcat(buf,"sending response\n");
		udp_write(fd, buf, strlen(buf));
	}

	return 0;
}
#else
int main(int argc, char *argv[]) {
	int fd = udp_socket_init("127.0.0.1", 5555);
	unsigned char buf[BUFSIZE];
	int recvlen;

	if(argc>1) {
		udp_write(fd, argv[1], strlen(argv[1]));
	} else {
		char *str = "test string\n";
		char *str2 = "nog een\n";
		if (udp_write(fd, str, strlen(str)) < 0)
			perror("write");
//		if (udp_write(fd, str2, strlen(str2)) < 0)
//			perror("write");
		recvlen = udp_read(fd, buf, sizeof(buf), 1);

	}
	if (recvlen) {
		buf[recvlen]='\0';
		printf("client: %d %s\n",recvlen,buf);
	}

	return 0;
}
#endif
#endif //LXSCRIBO
