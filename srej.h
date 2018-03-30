
#ifndef _SREJ_H__
#define _SREJ_H__

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
#include <sys/wait.h>
#include "networks.h"
#include "cpe464.h"

#define MAX_NAME 100
#define MAX_LEN 1500
#define SIZE_OF_BUF_SIZE 4
#define MAX_TRIES 10
#define LONG_TIME 10
#define SHORT_TIME 1

#pragma pack (1)

typedef struct {
    uint32_t seq_num;
    uint16_t checksum;
    uint8_t flag;
} Header;

typedef enum FLAG {
    FLG_C2S_INIT = 1,
    FLG_S2C_INIT = 2,
    FLG_DATA = 3,
    FLG_NONUSED = 4,
    FLG_RR = 5,
    FLG_SREJ = 6,
    FLG_C2S_FNAME = 7,
    FLG_S2C_GOOD_FNAME = 8,
    FLG_S2C_BAD_FNAME = 9,
    FLG_S2C_EOF = 10,
    FLG_C2S_EOF_ACK = 11,
    FLG_CRC_ERROR = 12
} FLAG;

int createHeader(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, 
 uint8_t flag, uint32_t seq_num, uint8_t *packet);
int32_t recv_buf(uint8_t *buf, uint32_t len, int32_t recv_sk_num, 
 Connection *connection, uint8_t *flag, uint32_t *seq_num);
int retrieveHeader(char *data_buf, int recv_len, uint8_t *flag, 
 uint32_t *seq_num);
int processSelect(Connection *client, int *retryCount, int selectTimeoutState, 
 int dataReadyState, int doneState);

#endif
