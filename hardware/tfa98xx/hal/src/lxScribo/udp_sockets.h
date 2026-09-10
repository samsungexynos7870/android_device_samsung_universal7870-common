#ifndef UDPSOCKETS_H_
#define UDPSOCKETS_H_

/*
 * Unix variant of udp_read()
 */
int udp_read(int fd, char *inbuf, int len, int waitsec);

/*
 * Unix variant of udp_write()
 */
int udp_write(int fd, char *outbuf, int len);

#endif /* UDPSOCKETS_H_ */
