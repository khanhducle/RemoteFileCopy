#include "networks.h"
#include "cpe464.h"
#include "srej.h"
#include "window.h"

#define START_SEQ_NUM 1111

enum State {
   START, FILENAME, FILE_OK, RECV_DATA, SEND_ACK, DONE
};

typedef enum State STATE;

STATE Start(char ** argv, Connection *server, 
 uint32_t *seqNumSend, uint32_t *seqNumRecv);
STATE Filename(char *fname, int32_t winSize, int32_t bufSize, 
 Connection *server, uint32_t *seqNumSend, uint32_t *seqNumRecv);
STATE File_OK(int *output_file_fd, char *outputFileName);
STATE RecvData(int32_t outFd, Connection *server, Window *w, 
 uint32_t *seqNumRecv);
STATE SendACK(int32_t output_file, Connection *server, 
 Window *win, uint32_t *seqNumSend);
void CheckArgs(int argc, char ** argv);

int main(int argc, char **argv) {
   Connection server;
   int32_t output_file_fd = 0;
   uint32_t seqNumSend = START_SEQ_NUM;
   uint32_t seqNumRecv = 0;
   STATE state = START;
   Window *win = NULL;

   CheckArgs(argc, argv);
   sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	
   while (state != DONE) {
      switch (state) {
         case START:
	    state = Start(argv, &server, &seqNumSend, &seqNumRecv);
	    break;
			
	 case FILENAME:
	    state = Filename(argv[2], atoi(argv[3]), 
	     atoi(argv[4]), &server, &seqNumSend, &seqNumRecv);
	    break;
			
	 case FILE_OK:
	    state = File_OK(&output_file_fd, argv[1]);
	    win = (Window *)CreateWindow(atoi(argv[3]), seqNumRecv + 1);		
	    break;
			
	 case RECV_DATA:
	    state = RecvData(output_file_fd, &server, win, &seqNumRecv);
	    break;
			
	 case SEND_ACK:
	    state = SendACK(output_file_fd, &server, win, &seqNumSend);
	    break;
			
	 case DONE:
            FreeWindow(win);
            close(server.sk_num);
	    break;
			
	 default:
            state = DONE;
	    break;
      }
   }
	
   return 0;
}

STATE Start(char **argv, Connection *server, uint32_t *seqNumSend, 
 uint32_t *seqNumRecv) {
   STATE retVal = START;
   uint8_t flag = 0;
   uint8_t packet[MAX_LEN];
   int32_t recvCheck = 0;
   static int retryCount = 0;

   if (server->sk_num > 0) {
      close(server->sk_num);
   }
	
   if (udp_client_setup(argv[6], atoi(argv[7]), server) < 0) {
      return DONE;
   }
	
   send_buf(NULL, 0, server, FLG_C2S_INIT, *seqNumSend, packet);
   
   if ((retVal = (STATE) processSelect(server, &retryCount, 
    START, FILENAME, DONE)) == FILENAME) {
      recvCheck = recv_buf(packet, MAX_LEN, server->sk_num, server, 
       &flag, seqNumRecv); 
      if (recvCheck == FLG_CRC_ERROR || flag != FLG_S2C_INIT) {
	 retVal = START;
      }	
      else {
         (*seqNumSend)++;
      }
   }
   return retVal;
}

STATE Filename(char *fname, int32_t winSize, int32_t bufSize, 
 Connection *server, uint32_t *seqNumSend, uint32_t *seqNumRecv) {
   STATE retVal = START;
   uint8_t packet[MAX_LEN], buf[MAX_LEN];
   uint8_t flag = 0;
   int32_t fname_len = strlen(fname) + 1;
   int32_t recvCheck = 0;
   static int retryCount = 0;
	
   winSize = htonl(winSize);
   memcpy(buf, &winSize, sizeof(winSize));
   bufSize = htonl(bufSize);
   memcpy(buf + sizeof(winSize), &bufSize, sizeof(bufSize));
   memcpy(buf + sizeof(winSize) + sizeof(bufSize), fname, fname_len);
	
   send_buf(buf, fname_len + sizeof(winSize) + sizeof(bufSize), server, 
    FLG_C2S_FNAME, *seqNumSend, packet);
	
   if ((retVal = (STATE) processSelect(server, &retryCount, 
    START, FILE_OK, DONE)) == FILE_OK) {
      recvCheck = recv_buf(packet, MAX_LEN, server->sk_num, server, 
       &flag, seqNumRecv);
		
      if (recvCheck == FLG_CRC_ERROR) {
	 retVal = START;
      }
      else if (flag == FLG_S2C_BAD_FNAME) {
	 retVal = DONE;
      }
      else if (flag == FLG_S2C_GOOD_FNAME) {
         (*seqNumSend)++;
      }
      else {
         retVal = FILENAME;
      }
   }	
   return retVal;
}

STATE File_OK(int *output_file_fd, char *outputFileName) {
   STATE retVal = RECV_DATA;
	
   if ((*output_file_fd = open(outputFileName, 
    O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) {
      perror("File open error: ");
      retVal = DONE;
   }
   return retVal;
}

STATE RecvData(int32_t outFd, Connection *server, Window *w, uint32_t *seqNumRecv) {
   STATE retVal = RECV_DATA;
   int32_t recvCheck;
   uint8_t buf[MAX_LEN];
   uint8_t flag;
   static int retryCount = 0;
   Segment *seg = NULL;

   if ((retVal = (STATE) processSelect(server, &retryCount, 
    RECV_DATA, SEND_ACK, DONE)) == SEND_ACK) {
      recvCheck = recv_buf(buf, MAX_LEN, server->sk_num, server, 
       &flag, seqNumRecv);
      
      if (recvCheck != FLG_CRC_ERROR) {
         if (flag == FLG_DATA) {
            AddSegment(w, buf, recvCheck, *seqNumRecv); 
         }
         if (flag == FLG_S2C_EOF) {
	    w->endSeq = *seqNumRecv;
         }
 
         while((seg = w->segment + w->start) && seg->hasData) {
            write(outFd, seg->buf, seg->segSize);
            seg->hasData = 0;
            w->curSize--;
            w->startSeq++;
            w->start = (w->start + 1) % w->winSize;
         }
      }
   }
   if (retVal == DONE) {
      close(outFd);
   }
   return retVal;
}

STATE SendACK(int32_t output_file, Connection *server, Window *w, 
 uint32_t *seqNumSend) {
   STATE retVal = RECV_DATA;
   uint8_t buf[MAX_LEN], packet[MAX_LEN];
   uint8_t flag = 0;
   uint32_t seqNum;
   int32_t bufSize = sizeof(seqNum);

   if (w->endSeq == w->startSeq) {
      bufSize = 0;
      close(output_file);
      flag = FLG_C2S_EOF_ACK;
      retVal = DONE;
   }
   else if (w->curSize) {
      flag = FLG_SREJ;
   }
   else {
      flag = FLG_RR;
   }
   
   seqNum = w->startSeq;
   seqNum = htonl(seqNum);
   memcpy(buf, &seqNum, sizeof(seqNum));
 
   send_buf(buf, bufSize, server, flag, *seqNumSend, packet);
   (*seqNumSend)++;
   return retVal;
}

void CheckArgs(int argc, char ** argv) {
   if (argc != 8) {
      printf("Lack of Arguments.\n");
      exit(-1);
   }
	
   if (strlen(argv[1]) > 100) {
      printf("Local filename must be < 100\n");
      exit(-1);
   }
	
   if (strlen(argv[2]) > 100) {
      printf("Remote filename must be < 100\n");
      exit(-1);
   }
	
   if (atoi(argv[3]) < 1) {
      printf("Invalid window size\n");
      exit(-1);
   }
	
   if (atoi(argv[4]) < 400 || atoi(argv[4]) > 1400) {
      printf("buffer size must be between 400 and 1400\n");
      exit(-1);
   }
	
   if (atof(argv[5]) < 0 || atof(argv[5]) > 1) {
      printf("Error rate must be between 0 and 1\n");
      exit(-1);
   }	
}

