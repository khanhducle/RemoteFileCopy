#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <stdlib.h>
#include <stdint.h>
#include "srej.h"

typedef struct {
   uint32_t seqNum;
   int32_t segSize;
   uint8_t buf[MAX_LEN];
   int32_t hasData;
} Segment;

typedef struct {
   Segment *segment;  
   int32_t winSize;
   int32_t curSize;
   int32_t start; // start Idx
   uint32_t startSeq;
   uint32_t endSeq;
} Window;

void *CreateWindow(int32_t winSize, uint32_t seqNum);
int isValidSeqNum(void *window, uint32_t seqNum);
int isClosed(void *window);
void AddSegment(void *window, uint8_t *buf, uint32_t bufLen, uint32_t seqNum); 
void MoveWindow(void *window, uint32_t seqNum);
void FreeWindow(void *window);

#endif
