#include "networks.h"
#include "srej.h"
#include "cpe464.h"
#include "window.h"

#define START_SEQ_NUM 2222

enum State {
   START, DONE, FILENAME, SEND_DATA, WAIT_ON_ACK, TIMEOUT_ON_ACK
};

typedef enum State STATE;

void process_server(int server_sk_num, char **argv);
void process_client(int32_t server_sk_num, uint8_t *buf, Connection *client);
STATE start(Connection *client, uint8_t *packet, uint32_t *seqNumSend);
STATE filename(Connection *client, uint8_t *buf, int32_t *bufSize, 
int32_t *data_file, Window **window, uint32_t *seqNumSend, uint32_t *seqNumRecv);
STATE send_data(Connection *client, uint8_t *packet, int32_t data_file, 
 uint32_t *seqNumSend, Window *w, int32_t bufSize, int32_t *eof);
STATE wait_on_ack(Connection *client, Window *w, uint8_t *packet, 
 int32_t data_file, int32_t eof);
STATE timeout_on_ack(Connection *client, Window *w, uint8_t *packet, 
 uint32_t seqNumSend, int32_t eof);
int check_args(int argc, char **argv);

int main(int argc, char **argv) {
   int32_t server_sk_num = 0;
   int portNumber = 0;

   portNumber = check_args(argc, argv);
   server_sk_num = udp_server(portNumber);
   process_server(server_sk_num, argv);

   return 0;
}

void process_server(int server_sk_num, char **argv) {
   pid_t pid = 0;
   int status = 0;
   uint8_t buf[MAX_LEN];
   Connection client;
   uint8_t flag = 0;
   uint32_t seq_num = 0;
   int32_t recv_len = 0;

   while(1) 
   {
      if (select_call(server_sk_num, LONG_TIME, 0, SET_NULL) == 1) {
         recv_len = recv_buf(buf, MAX_LEN, server_sk_num, &client, &flag, &seq_num);
         if (recv_len != FLG_CRC_ERROR && recv_len == 0 && flag == FLG_C2S_INIT) {
            if ((pid = fork()) < 0) { // parent
               perror("fork");
               exit(-1);
            }
            
            if (pid == 0) { // child
               sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
               process_client(server_sk_num, buf, &client);
               exit(0);
            }
         }

         while(waitpid(-1, &status, WNOHANG) < 0);
      }
   }
}

void process_client(int32_t server_sk_num, uint8_t *buf, Connection *client) {
   STATE state = START;
   int32_t data_file = 0, bufSize = 0;
   uint8_t packet[MAX_LEN];
   uint32_t seqNumSend = START_SEQ_NUM;
   uint32_t seqNumRecv = START_SEQ_NUM;
   Window *window = NULL;
   int32_t eof = 0;
   
   while (state != DONE) {
      switch(state) {
         case START:
            state = start(client, packet, &seqNumSend);
            break;
         
         case FILENAME:
            state = filename(client, buf, &bufSize, &data_file, &window,
             &seqNumSend, &seqNumRecv);
            break;

         case SEND_DATA:
            state = send_data(client, packet, data_file, &seqNumSend, 
             window, bufSize, &eof);
            break;
 
         case WAIT_ON_ACK:
            state = wait_on_ack(client, window, packet, data_file, eof);
            break;

         case TIMEOUT_ON_ACK:
            state = timeout_on_ack(client, window, packet, seqNumSend, eof);
            break;
 
         case DONE:
            FreeWindow(window);
            close(client->sk_num);
            break;

         default:
            state = DONE;
            break;
      }
   }
}

STATE start(Connection *client, uint8_t *packet, uint32_t *seqNumSend) {
   if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("filename, open client socket");
      exit(-1);
   }
   send_buf(NULL, 0, client, FLG_S2C_INIT, *seqNumSend, packet);
   (*seqNumSend)++;
   return FILENAME;
}

STATE filename(Connection *client, uint8_t *buf, int32_t *bufSize, 
 int32_t *data_file, Window **window, uint32_t *seqNumSend, uint32_t *seqNumRecv) {
   int32_t winSize = 0;
   int32_t recv_len = 0;
   uint8_t flag;
   char fname[MAX_LEN];
   STATE retVal = DONE;
   
   if (select_call(client->sk_num, LONG_TIME, 0, NOT_NULL) == 0) {
      return retVal;
   }
   recv_len = recv_buf(buf, MAX_LEN, client->sk_num, client, &flag, seqNumRecv);
   if (recv_len == FLG_CRC_ERROR) {
      return retVal;
   }

   if (flag == FLG_C2S_FNAME) {
      memcpy(&winSize, buf, sizeof(winSize));
      winSize = ntohl(winSize);

      memcpy(bufSize, buf + sizeof(winSize), sizeof(*bufSize));
      *bufSize = ntohl(*bufSize);

      memcpy(fname, buf + sizeof(winSize) + sizeof(*bufSize), 
       recv_len - sizeof(winSize) - sizeof(*bufSize));
     
      if ((*data_file = open(fname, O_RDONLY)) < 0) {
         send_buf(NULL, 0, client, FLG_S2C_BAD_FNAME, *seqNumSend, buf);
         retVal = DONE;
      }
      else {
         send_buf(NULL, 0, client, FLG_S2C_GOOD_FNAME, *seqNumSend, buf);
         (*seqNumSend)++;
         *window = (Window *)CreateWindow(winSize, *seqNumSend);
         retVal = SEND_DATA;     
      }
   }
   return retVal;
}

STATE send_data(Connection *client, uint8_t *packet, int32_t data_file, 
 uint32_t *seqNumSend, Window *w, int32_t bufSize, int32_t *eof) {
   uint8_t buf[MAX_LEN];
   int32_t len_read = 0;
   STATE retVal = WAIT_ON_ACK;
   if (*eof == 1 || isClosed(w)) {
      return retVal;
   }
   
   len_read = read(data_file, buf, bufSize);
   switch(len_read) {
      case -1:
         perror("SEND DATA: read error\n");
         close(data_file);
         retVal = DONE;
         break;

      case 0:
         *eof = 1;
         send_buf(NULL, 0, client, FLG_S2C_EOF, *seqNumSend, packet);
         break;
   
      default:
         AddSegment(w, buf, len_read, *seqNumSend);
         send_buf(buf, len_read, client, FLG_DATA, *seqNumSend, packet);
         (*seqNumSend)++;
         break;
   }
   return retVal;
}

STATE wait_on_ack(Connection *client, Window *w, uint8_t *packet, 
 int32_t data_file, int32_t eof) {
   STATE retVal = SEND_DATA;
   uint8_t buf[MAX_LEN];
   uint32_t seqNumRecv = 0, seqNumAck = 0;
   static int retryCount = 0;
   int32_t recvFlag = 0, recv_len = 0;
   uint8_t flag = 0;
   Segment *seg = NULL;

   if(eof == 1 || isClosed(w)) {
      if ((retVal = (STATE) processSelect(client, &retryCount, 
       TIMEOUT_ON_ACK, SEND_DATA, DONE)) == SEND_DATA) {
         recvFlag = 1;
      } 
   }
   else {
      if (select_call(client->sk_num, 0, 0, NOT_NULL) == 1) {
         recvFlag = 1;
      }      
   }

   if (recvFlag == 1) {
      recv_len = recv_buf(buf, MAX_LEN, client->sk_num, client,  
       &flag, &seqNumRecv);
      if (recv_len == FLG_CRC_ERROR) {
         retVal = SEND_DATA;
      }
      else if (flag == FLG_C2S_EOF_ACK) {
         retVal = DONE;
         close(data_file);
      }
      else if (flag == FLG_RR || flag == FLG_SREJ) {
         memcpy(&seqNumAck, buf, SIZE_OF_BUF_SIZE);
         seqNumAck = ntohl(seqNumAck);
         MoveWindow(w, seqNumAck);
         if (flag == FLG_SREJ) {
            seg = w->segment + w->start;
            send_buf(seg->buf, seg->segSize, client, FLG_DATA, 
             seg->seqNum, packet);
         }
      }
   }
   return retVal;
}

STATE timeout_on_ack(Connection *client, Window *w, uint8_t *packet, 
 uint32_t seqNumSend, int32_t eof) {
   Segment *seg = NULL;
   if (!eof || w->curSize) {
      seg = w->segment + w->start;
      send_buf(seg->buf, seg->segSize, client, FLG_DATA, seg->seqNum, packet);
   }
   else {
      send_buf(NULL, 0, client, FLG_S2C_EOF, seqNumSend, packet);
   }
   return WAIT_ON_ACK;   
}

int check_args(int argc, char **argv) {
   int portNumber = 0;
   if (argc < 2 || argc > 3) {
      perror("Invalid input");
      exit(-1);
   }

   if (argc == 3) {
      portNumber = atoi(argv[2]);
   }
   else {
      portNumber = 0;
   }

   return portNumber;
}

