
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/

#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#define MAX_LEN 1500

enum SELECT {
   SET_NULL, NOT_NULL
};

typedef struct {
   int32_t sk_num;
   struct sockaddr_in remote;
   uint32_t len;
} Connection;

int32_t udp_server(int portNumber);
int32_t udp_client_setup(char *hostname, uint16_t port_num, Connection *connection);
int32_t select_call(int32_t socket_num, int32_t seconds, int32_t microseconds, int32_t set_null);
int32_t safeSend(uint8_t *packet, uint32_t len, Connection *connection);
int32_t safeRecv(int recv_sk_num, char *data_buf, int len, Connection *connection);

#endif
