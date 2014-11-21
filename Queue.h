//
//  Queue.h
//  test
//
//  Created by Jian Qiao on 8/30/14.
//  Copyright (c) 2014 Jian Qiao. All rights reserved.
//

#ifndef test_Queue_h
#define test_Queue_h
#include "global.h"
#include "syscalls.h"
#include "z502.h"
//#include  <sys/resource.h>
//#include <malloc/malloc.h>
#include <stdlib.h>
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
INT32 LockResult, LockResult2, LockResultPrinter, TimeLockResult;
//#include "protos.h"

//typedef         int                             INT32;
typedef         struct Node *PCB;
typedef         struct message *MSG;
typedef         struct disk     *DISK;

typedef struct DiskQueue{
    DISK front;
    DISK rear;
    INT32 size;
}DiskQueue;

typedef struct DiskStack{
    DISK top;
   // DISK rear;
    INT32 size;
}DiskStack;


typedef struct disk
{
    int readOrWrite;
    long diskID;    // diskID may not be used here
    long sectorID;
    PCB PCB;
    int alreadyGetDisk;
    //INT32 readOrWrite;
    DISK next;
    
} disk;

typedef struct
{
    long frameID;
    long pageID;
    long pid;
    int isAvailable;
} FRAME;

typedef struct Node {
    long pid;
    void *context;
    char name[140];
    int prior;
    long diskID;
    long sector;
    INT32 wakeuptime;
    PCB next;
    
}Node;

typedef struct Queue{
    PCB front;
    PCB rear;
    INT32 size;
}Queue;

typedef struct MsgQueue{
    MSG front;
    MSG rear;
    INT32 size;
}MsgQueue;

typedef struct message {
    long sid;
	long tid;
	INT32 length;
	char msg[100];
    MSG next;
    
}message;


Queue *InitQueue();

Node *InitPCB(INT32 wakeuptime);

INT32 *InitPCB2(SYSTEM_CALL_DATA *SystemCallData,PCB pnode);

INT32 IsEmpty(Queue *queue);

PCB EnQueue(Queue *queue,PCB pnode);

PCB DeQueue(Queue *queue);

PCB DeQueueWithoutFree(Queue *queue);

void EnQueueWithPrior(Queue *queue,PCB pnode);

void EnQueueWithwakeUpTime(Queue *queue,PCB pnode);

void TerminateSelf(Queue *queue,PCB pnode);

PCB DeleWithoutFree(Queue *queue,PCB pnode);

MSG EnQueueMsg(MsgQueue *queue,MSG pnode);

MsgQueue *InitMsgQueue();

MSG DeQueueMsg(MsgQueue *queue);

DiskQueue *InitDiskQueue();

//2

DiskStack *InitDiskStack();
DiskQueue *InitDiskQueue();


DISK InitDisk(long diskID,long sectorID, int readOrWrite,PCB pnode);
void enStack(DiskStack *stack,DISK disk);
DISK popStack(DiskStack *stack);

DISK EnQueueDisk(DiskQueue *queue,DISK pnode);

#endif
