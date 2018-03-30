#include "networks.h"
#include "cpe464.h"
#include "srej.h"

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
    int32_t sentLen = 0;
    int32_t sendingLen = 0;
    
    //set up the packet (seq #, crc, flag, data)
    if (len > 0) {
        memcpy(&packet[sizeof(Header)], buf, len);
    }
    sendingLen = createHeader(len, flag, seq_num, packet);
    sentLen = safeSend(packet, sendingLen, connection);
    return sentLen;
}

int createHeader(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
    //creates the regular header (puts in packet) including
    //the seq number, flag, and checksum
    Header * aHeader = (Header *) packet;
    uint16_t checksum = 0;
    seq_num = htonl(seq_num);
    memcpy(&(aHeader->seq_num), &seq_num, sizeof(seq_num));
    
    aHeader->flag = flag;
    memset(&(aHeader->checksum), 0, sizeof(checksum));
    checksum = in_cksum((unsigned short *) packet, len + sizeof(Header));
    memcpy(&(aHeader->checksum), &checksum, sizeof(checksum));
    
    return len + sizeof(Header);
}

int32_t recv_buf(uint8_t *buf, uint32_t len, int32_t recv_sk_num, Connection *connection, uint8_t *flag, uint32_t *seq_num) {
    char data_buf[MAX_LEN];
    int32_t recv_len = 0;
    int32_t dataLen = 0;
    
    recv_len = safeRecv(recv_sk_num, data_buf, len, connection);
    dataLen = retrieveHeader(data_buf, recv_len, flag, seq_num);
    
    //dataLen could be -1 if crc error or if no data
    if (dataLen > 0) {
        memcpy(buf, &data_buf[sizeof(Header)], dataLen);
    }
    return dataLen;
}

int retrieveHeader(char *data_buf, int recv_len, uint8_t *flag, uint32_t *seq_num) {
    Header *aHeader = (Header *) data_buf;
    int returnVal = 0;
    
    if (in_cksum((unsigned short *) data_buf, recv_len) != 0) {
        returnVal = FLG_CRC_ERROR;
    }
    else {
        *flag = aHeader->flag;
        memcpy(seq_num, &(aHeader->seq_num), sizeof(aHeader->seq_num));
        *seq_num = ntohl(*seq_num);
        returnVal = recv_len - sizeof(Header);
    }
    return returnVal;
}

int processSelect(Connection *client, int *retryCount, int selectTimeoutState, int dataReadyState, int doneState) {
    //Returns
    //doneState if calling this function exceeds MAX_TRIES
    //selectTimeoutState if the select times out without receiving anything
    ///dataReadyState if select() returns indicating that data is ready for read
    
    int returnVal = dataReadyState;
    
    (*retryCount)++;
    
    if (*retryCount >= MAX_TRIES) {
        printf("Sent data %d times, no ACK, client is probably gone - so I am terminating\n", MAX_TRIES);
        returnVal = doneState;
    }
    else {
        if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
            *retryCount = 0;
            returnVal = dataReadyState;
        }
        else {
            //no data ready
            returnVal = selectTimeoutState;
        }
    }
    return returnVal;
}
