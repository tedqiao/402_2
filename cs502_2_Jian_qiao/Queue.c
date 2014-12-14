//
//  Queue.c
//  test
//
//  Created by Jian Qiao on 8/30/14.
//  Copyright (c) 2014 Jian Qiao. All rights reserved.
//

#include <stdio.h>
#include "Queue.h"






Queue *InitQueue(){
    Queue *pqueue=(Queue*)malloc(sizeof(Queue));
    if (pqueue!=NULL) {
        pqueue->front=NULL;
        pqueue->rear =NULL;
        pqueue->size =0;
    }
    return pqueue;
}

MsgQueue *InitMsgQueue(){
    MsgQueue *pqueue=(MsgQueue*)malloc(sizeof(MsgQueue));
    if (pqueue!=NULL) {
        pqueue->front=NULL;
        pqueue->rear =NULL;
        pqueue->size =0;
    }
    return pqueue;
}

DiskStack *InitDiskStack(){
    DiskStack *pstack=(DiskStack*)malloc(sizeof(DiskStack));
    if (pstack!=NULL) {
        pstack->top = NULL;
        pstack->size=0;
    }
    return pstack;
}

void enStack(DiskStack *stack,DISK disk){
    if (disk!=NULL) {
        if (stack->top==NULL) {
            stack->top=disk;
        }else{
        disk->next=stack->top;
        stack->top=disk;
        }
        stack->size++;
    }
}
DISK popStack(DiskStack *stack){
    DISK tmp;
    if (stack->top!=NULL) {
        
        tmp=stack->top;
        stack->top=stack->top->next;
        return tmp;
        stack->size--;
    }
    else{
        return NULL;
    }
    
}


Node *InitPCB(INT32 wakeuptime){
    PCB pnode = (PCB)malloc(sizeof(Node));
    pnode->wakeuptime = wakeuptime;
    pnode->next = NULL;
    return pnode;
}

INT32 *InitPCB2(SYSTEM_CALL_DATA *SystemCallData,PCB pnode){
    //PCB pnode = (PCB)malloc(sizeof(Node));
    void *next_context;
    strcpy(pnode->name,(char*)SystemCallData->Argument[0]);
    Z502MakeContext(&next_context,(void *)SystemCallData->Argument[1],USER_MODE);
    pnode->context = next_context;
    pnode->prior = (int)SystemCallData->Argument[2];
    return 1;
}

INT32 IsEmpty(Queue *queue){
    if(queue->size==0&&queue->front==NULL)
        return 1;
    else
        return 0;
    }


PCB EnQueue(Queue *queue,PCB pnode){
   READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    if(pnode != NULL)
    {
        if(IsEmpty(queue))
        {
            queue->front = pnode;
        }
        else
        {
            queue->rear->next = pnode;
        }
        queue->rear = pnode;
        
        queue->size++;
    }
    //pnode->next=NULL;
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);

    return pnode;

}

MSG EnQueueMsg(MsgQueue *queue,MSG pnode){
    
    if(pnode != NULL)
    {
        if(queue->front==NULL)
        {
            queue->front = pnode;
        }
        else
        {
            queue->rear->next = pnode;
        }
        queue->rear = pnode;
        queue->size++;
        
    }
    pnode->next=NULL;
    return pnode;
    
}

void EnQueueWithwakeUpTime(Queue *queue,PCB pnode){
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    PCB pprenode;
    
    if(pnode != NULL)
    {
        if(IsEmpty(queue))
        {
            queue->front = pnode;
            queue->rear = pnode;
        }
        else
        {
            
            pprenode = queue->front;
            
            //printf("-----------i am main pointer  %d\n",queue->size);
            if (pnode->wakeuptime<queue->front->wakeuptime) {
                //printf("--------i am pinter1\n");
                queue->front = pnode;
                pnode->next = pprenode;
                
            }
            
            else if(pprenode->next==NULL){
                //printf("--------i am pinter2\n");
                queue->rear->next = pnode;
                queue->rear = pnode;
                pnode->next=NULL;
            }
            else {
                //printf("--------i am pinter3\n");
                while(pprenode!=NULL&&pprenode->next!=NULL){
                    
                    if(pnode->wakeuptime<pprenode->next->wakeuptime){
                        pnode->next = pprenode->next;
                        pprenode->next = pnode;
                        break;
                    }
                    pprenode = pprenode->next;
                }
                if(pprenode->next==NULL){
                    queue->rear->next = pnode;
                    queue->rear = pnode;
                    // pnode->next=NULL;
                }
            }
            
        }
        //queue->rear = pnode;
        queue->size++;
    }
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    //return pnode;
}

void EnQueueWithPrior(Queue *queue,PCB pnode){
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    PCB pprenode;
    
    if(pnode != NULL)
    {
        if(IsEmpty(queue))
        {
            queue->front = pnode;
            queue->rear = pnode;
        }
        else
        {
            
            pprenode = queue->front;
            if (pnode->prior<queue->front->prior) {
                queue->front = pnode;
                pnode->next = pprenode;
                
            }
            
            else if(pprenode->next==NULL){
                queue->rear->next = pnode;
                queue->rear = pnode;
                pnode->next=NULL;
            }
            else {
               
                while(pprenode!=NULL&&pprenode->next!=NULL){
                    
                    if(pnode->prior<pprenode->next->prior){
                        pnode->next = pprenode->next;
                        pprenode->next = pnode;
                        break;
                    }
                    pprenode = pprenode->next;
                }
                if(pprenode->next==NULL){
                    queue->rear->next = pnode;
                    queue->rear = pnode;
                   // pnode->next=NULL;
                }
            }
            
        }
        //queue->rear = pnode;
        queue->size++;
    }
    //return pnode;
      READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
}

PCB DeQueue(Queue *queue){
    PCB pnode = queue->front;
    if(IsEmpty(queue)!=1&&pnode!=NULL)
    {
        queue->size--;
        queue->front = pnode->next;
        free(pnode);
        if(queue->size==0)
        queue->rear = NULL;
    }
    return queue->front;
}
MSG DeQueueMsg(MsgQueue *queue){
    MSG pnode = queue->front;
    if(pnode!=NULL)
    {
        queue->size--;
        queue->front = pnode->next;
        free(pnode);
        if(queue->size==0)
            queue->rear = NULL;
    }
    return queue->front;
}

PCB DeQueueWithoutFree(Queue *queue){
     READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    PCB pnode = queue->front;
    if(IsEmpty(queue)!=1&&pnode!=NULL)
    {
        
        queue->size--;
        queue->front = pnode->next;
        
        pnode->next = NULL;
        
       // pnode;
        if(queue->size==0)
            
            queue->rear = NULL;
       // printf("am here\n");
    }
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    return pnode;
    
}

void TerminateSelf(Queue *queue,PCB pnode){
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);

    if (!IsEmpty(queue)) {
  
        PCB p1;
        PCB p2 = NULL;
        p1 = queue ->front;
        while (p1->pid!=pnode->pid&&p1->next!=NULL) {
            p2=p1;
            p1=p1->next;
        }
        if (p1->pid==pnode->pid) {
            if (p1==queue->front) {
                queue->front=p1->next;
            }
            else
                p2->next=p1->next;
            queue->size--;
            free(p1);
        }
    /*if(tmp->pid==pnode->pid){
        queue->front = queue->front->next;
        free(tmp);
        queue->size--;
    }
    else{
    while (tmp->next!=NULL) {
        if (tmp->next->pid ==pnode->pid) {
            NEXT = tmp->next;
            free(tmp->next);
            tmp->next = NEXT->next;
            queue->size--;
            break;
        }
            tmp = tmp->next;
        
        
    }
    }*/
    }
    READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    
}

PCB DeleWithoutFree(Queue *queue,PCB pnode){
    if (!IsEmpty(queue)) {
        
        PCB p1;
        PCB p2 = NULL;
        p1 = queue ->front;
        while (p1->pid!=pnode->pid&&p1->next!=NULL) {
            p2=p1;
            p1=p1->next;
        }
        if (p1->pid==pnode->pid) {
            if (p1==queue->front) {
                queue->front=p1->next;
            }
            else if(p1->pid==queue->rear->pid){
                queue->rear = p2;
                queue->rear->next=NULL;
            }
            else
                p2->next=p1->next;
                queue->size--;
            p1->next=NULL;
            return p1;
        }
    }
    return NULL;
}

DiskQueue *InitDiskQueue(){
    DiskQueue *pqueue=(DiskQueue*)malloc(sizeof(DiskQueue)); ;
    if (pqueue!=NULL) {
        pqueue->front=NULL;
        pqueue->rear =NULL;
        pqueue->size =0;
    }
    return pqueue;
}


DISK EnQueueDisk(DiskQueue *queue,DISK pnode){
    
    if(pnode != NULL)
    {
        if(queue->front==NULL)
        {
            queue->front = pnode;
        }
        else
        {
            queue->rear->next = pnode;
        }
        
        queue->rear = pnode;
        queue->size++;
        
    }
    pnode->next=NULL;
    return pnode;
    
}


DISK InitDisk(PCB pnode,int token){
    DISK mydisk=(DISK)malloc(sizeof(disk));

    mydisk->GetDisk=token;
    mydisk->next=NULL;
    mydisk->PCB = pnode;
    //mydisk->PCB->pid=pnode->pid;
    //mydisk->PCB->context=pnode->context;
   // printf("%d       %d",&mydisk->PCB->context,&pnode->context);
    //mydisk->PCB->prior=pnode->prior;
   // strcpy(mydisk->PCB->name,pnode->name);
   // mydisk->PCB->pid=
    return mydisk;
}

DISK InitDisk2(long diskID,long sectorID, int readOrWrite,PCB pnode){
    DISK mydisk = (DISK)malloc(sizeof(disk));
    if (diskID<0) {
        return NULL;
    }
    mydisk->GetDisk=0;
    mydisk->next=NULL;
    mydisk->PCB = pnode;
    // mydisk->PCB->pid=
    return mydisk;
}

DISK EnQueueDiskHead(DiskQueue *queue,DISK pnode){
    DISK tmp;
    if(pnode != NULL)
    {
        
        if(queue->front==NULL)
        {
            
            queue->front = pnode;
            queue->rear = pnode;
            
        }
        else
        {
            tmp= queue->front;
            queue->front=pnode;
            queue->front->next=tmp;
            
            //queue->rear->next = pnode;
        }
        
        //queue->rear = pnode;
        queue->size++;
        
    }
        //pnode->next=NULL;
    
    return pnode;
}




/*int main(){
    PCB pcb1 = InitPCB(21);
    PCB pcb2 = InitPCB(22);
    PCB pcb3 = InitPCB(12);
    PCB pcb4 = InitPCB(30);
    Queue *queue = InitQueue();
    //INT32 i = 32,j=12,z=14;
    EnQueue(queue, pcb1);
    EnQueue(queue, pcb2);
    EnQueue(queue, pcb3);
    EnQueue(queue, pcb4);
   // EnQueue(queue, 1);
    //EnQueue(queue, 2);
   // EnQueue(queue, z);
    DeQueue(queue);
    DeQueue(queue);
    EnQueue(queue, pcb3);
    printf("%d %d\n",queue->rear->wakeuptime,queue->front->wakeuptime);
    return 0 ;
}*/

