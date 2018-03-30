#include "window.h"

void *CreateWindow(int32_t winSize, uint32_t seqNum) {
   Window *window = (Window *) calloc(1, sizeof(Window));
   window->segment = (Segment *) calloc(winSize, sizeof(Segment));
   window->winSize = winSize;
   window->start = seqNum % winSize;
   window->startSeq = seqNum;
   return window;
}

int isValidSeqNum(void *window, uint32_t seqNum) {
   Window *w = (Window *) window;
   uint32_t endSeq = w->startSeq + w->winSize;
   if (w->startSeq <= seqNum && seqNum < endSeq) {
      return 1;
   }
   return 0;
}

int isClosed(void *window) {
   Window *w = (Window *) window;
   if (w->winSize == w->curSize) {
      return 1;
   }
   return 0;
}

void AddSegment(void *window, uint8_t *buf, uint32_t bufLen, uint32_t seqNum) { 
   Window *w = (Window *) window;
   int i = seqNum % w->winSize;
   if (!isClosed(w) && isValidSeqNum(w, seqNum)) {
      (w->segment + i)->segSize = bufLen;
      (w->segment + i)->seqNum = seqNum;
      memcpy((w->segment + i)->buf, buf, bufLen);
      w->curSize++;
      (w->segment + i)->hasData = 1;
   }
}

void MoveWindow(void *window, uint32_t seqNum) {
   Window *w = (Window *) window;
   if (seqNum <= w->startSeq + w->curSize) {
      while(w->startSeq < seqNum) {
         (w->segment + w->start)->hasData = 0;
         w->start = (w->start + 1) % w->winSize;
         w->startSeq++;
         w->curSize--;
      }
   }
}
 
void FreeWindow(void *window) {
   Window *w = (Window *) window;                                   
   free(w->segment);                                       
   free(w);
}
