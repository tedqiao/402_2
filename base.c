/************************************************************************
 
 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.
 
 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to sample_code.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             "Queue.h"
#include             "z502.h"
#include             <pthread.h>
#include             <unistd.h>

#define         ILLEGAL_PRIORITY                -3
#define         LEGAL_PRIORITY                  10
#define         DUPLICATED                      -4
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define                     MAX_LENGTH              64
#define                     MAX_COUNT              106
#define					DISK_READ						0
#define					DISK_WRITE						1

// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;

extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
    "read_mod ", "get_time ", "sleep    ",
    "get_pid  ", "create   ", "term_proc",
    "suspend  ", "resume   ", "ch_prior ",
    "send     ", "receive  ", "disk_read",
    "disk_wrt ", "def_sh_ar" };

//test1111
char *test;
//
Queue *readyQueue;
Queue *timerQueue;
Queue *suspendQueue;
//DiskQueue *diskQueue;

MsgQueue *msgQueue;
DiskStack * diskStack;
INT32 LockResult, LockResult2, LockResultPrinter, TimeLockResult;
INT32 currentPCBnum=1;
INT32 currenttime = 0;
PCB startPCB;
PCB currentPCB;
PCB temp;
char action[8];
int printerClt=1;
UINT16 *PAGE_HEAD[10];
FRAME frame[PHYS_MEM_PGS];
DiskQueue *diskqueue[MAX_NUMBER_OF_DISKS];

long CreateProcess(PCB pnode);
INT32 GetIDByName(char* name);
int sendMessage(long sid,long tid,char* msg,int msglength);
int receiveMessage(long sid, char *msg,int msglength,long *actualLength, long *actualSid);
void schedule_printer();
void QueuePrinter(Queue *queue);
int  ChangePriorByID(long pid, int priority);
INT32  SuspendByID(long ID);
INT32  ResumeByID(long ID);



//==-----
void initFrame();
void dispatch();
void DISKreadOrWrite(long diskID, long sectorID, char* buffer, int readOrWrite);
void DiskQueuePrinter(DiskQueue *queue);

void dispatch(){
    while(readyQueue->front==NULL){
        int i;
        //break;
    
    currentPCB=NULL;
    for(i=1;i<2;i++){
        EnQueueWithPrior(readyQueue, diskqueue[i]->front->PCB);
        //schedule_printer("test", NULL);
    }
        Z502Idle();
    }
    
    //printf("%ld--------------------\n",readyQueue->front->pid);
    currentPCB=DeQueueWithoutFree(readyQueue);
    schedule_printer("change", currentPCB->pid);
    
    Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &currentPCB->context );
}


void DISKreadOrWrite(long diskID, long sectorID, char* buffer, int readOrWrite){
    int diskStatus;
    int deviceStatus;
    int i=1;
    
    MEM_WRITE(Z502DiskSetID, &diskID);
    
    MEM_READ(Z502DiskStatus, &diskStatus);
    
    if (diskStatus==DEVICE_FREE) {
        //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
        
        //do nothing
        //printf("i ama the %d----\n",diskStatus);
    }
    else{
        while(diskStatus==DEVICE_IN_USE) {
            //printf("i ama the %ld----\n",diskID);
            EnQueueDisk(diskqueue[(INT32)diskID], InitDisk(diskID,sectorID, readOrWrite,currentPCB));
            dispatch();
            CALL(MEM_WRITE(Z502DiskSetID, &diskID));
            MEM_READ(Z502DiskStatus, &diskStatus);
            
        }
    }
   //
   
    if (readOrWrite==DISK_WRITE) {
    //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
    CALL(MEM_WRITE(Z502DiskSetID, &diskID));
        
    //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
    CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
    CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
   
    CALL(MEM_WRITE(Z502DiskSetAction, &readOrWrite));
    diskStatus = 0;
    CALL(MEM_WRITE(Z502DiskStart, &diskStatus));
    }
    
    
    
 
    //printf("---------------------\n");
    if(readOrWrite==DISK_READ){
    CALL(MEM_WRITE(Z502DiskSetID, &diskID));
  
    CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
    CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
    
   
    CALL(MEM_WRITE(Z502DiskSetAction, &readOrWrite));
    diskStatus = 0;
    CALL(MEM_WRITE(Z502DiskStart, &diskStatus));
    
    }
    
    
    
    MEM_WRITE(Z502DiskSetID, &diskID);
    MEM_READ(Z502DiskStatus, &diskStatus);
 
    EnQueueDiskHead(diskqueue[(INT32)diskID], InitDisk(diskID,sectorID, readOrWrite,currentPCB));
    if(diskID==3)
    printf("id-1 %ld id-2 %ld id-3 %ld",diskqueue[1]->front->diskID,diskqueue[2]->front->diskID,diskqueue[3]->front->diskID);
    dispatch();
    Z502SwitchContext(SWITCH_CONTEXT_KILL_MODE, &currentPCB->context);

}


void memory_printer()
{
    int i = 0, state = 0;
    static int printCtl = 0;
    //if (enableMPrinter != 0 && printCtl % enableMPrinter == 0)
    //{
        READ_MODIFY(MEMORY_INTERLOCK_BASE + 14, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
        for (i = 0; i < (int)PHYS_MEM_PGS; i++)
        {
            if (frame[i].pageID >= 0 && frame[i].pageID <= 1023)
            {
                state = ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_VALID_BIT) >> 13) +
                ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_MODIFIED_BIT) >> 13) +
                ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_REFERENCED_BIT) >> 13);
                
                MP_setup(frame[i].frameID,frame[i].pid,frame[i].pageID,state);
            }
        }
        
        MP_print_line();
        // reset action to NULL
        
        printf("\n");
        READ_MODIFY(MEMORY_INTERLOCK_BASE + 14, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
   // }
    //printCtl++;
}

void initFrame(){
    int i;
    for (i= 0; i < (int)PHYS_MEM_PGS; i++)
    {
        frame[i].frameID = i;
        frame[i].isAvailable = 1;
        frame[i].pageID = -1;
    }
}




/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the OS.
 ************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    PCB                 tmp;
    INT32               sleepTime;
    //printf("i am the test2========\n");
    //printf("i am here----------\n");
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    
    printf( "Interrupt handler: Found device ID %d with status %d\n",device_id, status );
    
    if(device_id == TIMER_INTERRUPT){
        usleep(20);
        READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);

                //get current system time
               MEM_READ(Z502ClockStatus, &currenttime);
        
        while (timerQueue->front!=NULL) {
            tmp = timerQueue->front;
            //move the PCB whose wakeuptime is smaller than current system time from timerqueue to readyqueue
            if(tmp->wakeuptime<=currenttime){
                
                EnQueueWithPrior(readyQueue,DeQueueWithoutFree(timerQueue));
                schedule_printer("Inter",tmp->pid);
              
            }
            else{
                break;
            }
        }
            //reset system time if the timerqueue is not empty
            if (timerQueue->front!= NULL)
            {
                CALL(MEM_READ(Z502ClockStatus, &currenttime));
                sleepTime = timerQueue->front->wakeuptime - currenttime;
                CALL(MEM_WRITE(Z502TimerStart, &sleepTime));
            }
                READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
    }
   
   else if(device_id>TIMER_INTERRUPT&&device_id<=MAX_NUMBER_OF_DISKS+TIMER_INTERRUPT){
       
       
    }
    
   
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
 
}                                       /* End of interrupt_handler */
/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/


void    fault_handler( void )
{
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;
    static BOOL  Flag = FALSE;
    int             i;
    
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    
    printf( "Fault_handler: Found vector type %d with value %d\n",
           device_id, status );
    if(device_id == SOFTWARE_TRAP){//receive 0
        CALL(Z502Halt());
    }
    else if(device_id == CPU_ERROR){//receive 1
        CALL(Z502Halt());
    }
    else if(device_id == INVALID_MEMORY){//receive 2
        
        
        if (status >= VIRTUAL_MEM_PAGES){ //Address is larger than page table,
            
           // printf("hihihihihihihihihihihihih\n");
            CALL(Z502Halt());
        }
        
        if (status<0) {
            CALL(Z502Halt());
        }
        if (Z502_PAGE_TBL_ADDR == NULL ){ //Page table doesn't exist,
            
            Z502_PAGE_TBL_LENGTH = 1024;
            Z502_PAGE_TBL_ADDR = (UINT16 *)calloc( sizeof(UINT16), Z502_PAGE_TBL_LENGTH );
            PAGE_HEAD[currentPCB->pid] = Z502_PAGE_TBL_ADDR;
        }
        
        if (!Flag) {
            for (i=0; i<PHYS_MEM_PGS; i++) {
                if(frame[i].isAvailable==1){
                    frame[i].isAvailable=0;
                    frame[i].pageID = status;
                    frame[i].pid = currentPCB->pid;
                    Z502_PAGE_TBL_ADDR[status] = (UINT16)frame[i].frameID | 0x8000;
                    printf("-------------------frame[%ld] occupied by page %ld\n",frame[i].frameID
                           ,frame[i].pageID);
                    break;
                }
                
            }
            if (i==PHYS_MEM_PGS) {
                Flag = TRUE;
            }
            
        }
        
        //mark this page table slot as valid
        
        
        //printf("----------------%d\n",0x8000);
        
        
        if (status >= Z502_PAGE_TBL_LENGTH){//Address is larger than page table,
            
            CALL(Z502Halt());
        }
    }
    sleep(3);
    memory_printer();

}                                       /* End of fault_handler */

/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/
INT32 start_timer(long *SleepTime){
    
    INT32 Time=0;
    PCB tmp = currentPCB;
    if(SleepTime<0){// return error if the paremeter is invalid
        printf("Illegal sleeptime");
        return -1;
    }
    
    //get current absolute time, and set wakeUpTime attribute for currentPCB
    INT32 wakeuptime;
    CALL(MEM_READ(Z502ClockStatus, &Time));
    wakeuptime = Time + (INT32)SleepTime;
    
   
    tmp ->wakeuptime = wakeuptime;
    
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+6, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
    
    // add current node into timerqueue
    CALL(EnQueueWithwakeUpTime(timerQueue, tmp));
    
    
    CALL(MEM_WRITE(Z502TimerStart, &SleepTime));
    
    // if the readyqueue is empty keep idling until readyqueue is no longer empty, then pop the first PCB from readyqueue
    //and run it.
    while (readyQueue->front==NULL) {
            CALL(Z502Idle());
            schedule_printer("Idle", currentPCB->pid);
    }
    schedule_printer("sleep",currentPCB->pid);
    if(readyQueue->front!=NULL){
        currentPCB =  DeQueueWithoutFree(readyQueue);
        tmp = currentPCB;
        //currentPCBnum--;
    }
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+6, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult));
    usleep(50);
    CALL(Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(tmp->context)));
    
    return 0;
}

long CreateProcess(PCB pnode){
    
    PCB tmp = (PCB)malloc(sizeof(Node));
    
    if (pnode->prior == ILLEGAL_PRIORITY ) {// the PCB's priority is illegal then return error
        
        return ILLEGAL_PRIORITY;
    }
    
    tmp = readyQueue->front;
    
    if (currentPCBnum>15) {
        return -1;
    }
    
    while (tmp!=NULL) {// check if the PCB name is already exist
        
        if (strcmp(tmp->name,pnode->name)==0) {
            
           // printf("this is %d\n",pnode->prior);
            return DUPLICATED;
        }
       // printf("111111111111\n");
        tmp = tmp->next;
    }
        pnode->pid = (long)currentPCBnum;
    //enqueue the PCB base on its priority
    EnQueueWithPrior(readyQueue, pnode);
    currentPCBnum++;
    
    return pnode->pid;
}

INT32 GetIDByName(char* name){
    
    if (strcmp(name, "")==0) {// if the parameter 'name' is null system just return the name of PCB being running
        return (INT32)currentPCB->pid;
    }
    
    PCB tmp = (PCB)malloc(sizeof(Node));
    
    // check readqueue
    tmp = readyQueue->front;
    
    while (tmp!=NULL) {
        if (strcmp(tmp->name,name)==0) {
            printf("\n");
            return (INT32)tmp->pid;
        }
        //printf("11111111111\n");
        tmp = tmp->next;
    }
    
    // check timerqueue
    tmp = timerQueue->front;
    
    while (tmp!=NULL) {
        if (strcmp(tmp->name,name)==0) {
            printf("\n");
            return (INT32)tmp->pid;
        }
        //printf("222222222222\n");
        tmp = tmp->next;
    }
    
    return 15;
}

INT32  SuspendByID(long ID){
    
   // printf("-----------------this ID is %ld\n",ID);
    //CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+6, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
    if(ID == -1)//in my PCB i will not allow to syspend itself
	{
        printf("----------Dont allow to suspend self\n");
		return 0;
	}
    
   // printf("-------i here iam suspend-----\n");
    PCB p1;
    PCB p2;
    PCB tmp;
    if(ID>15){//if the ID is larger than 15 that is the maximum number of PCB in my system system will return an error
        printf("----------Illegal ID\n");
        return 0;
        
    }
    //to check if the target PCB is already in suspendqueue
    tmp = suspendQueue->front;
    while (tmp!=NULL ) {
        
        if(tmp->pid == ID){
            printf("----------its already in suspendqueue\n");
            return 0;
        }
        tmp = tmp->next;
    }
    
    //search for the target PCB by its ID
    if (!IsEmpty(readyQueue)) {
    p1 = readyQueue->front;
     //search readyqueue
    while (p1->pid!=ID&&p1->next!=NULL) {
        p2=p1;
        p1 = p1->next;
    }
    // if the PCB is found pop it from timerqueue then push it into suspendqueue
    if(p1->pid==ID){
        EnQueue(suspendQueue, DeleWithoutFree(readyQueue, p1));
        return 1;
    }
        
    }
    
    //search timerqueue
    if (!IsEmpty(timerQueue)) {
    p1 = timerQueue->front;
    while (p1->pid!=ID&&p1->next!=NULL) {
        p2=p1;
        p1 = p1->next;
    }
    // if the PCB is found pop it from timerqueue then push it into suspendqueue
    if(p1->pid==ID){
        EnQueue(suspendQueue, DeleWithoutFree(timerQueue, p1));
         return 1;
    }
    //printf("wo shi suspendqueue--->%s\n",suspendQueue->front->name);
       
    }
    
    return 0;
}

INT32  ResumeByID(long ID){
    PCB p1;
    PCB p2;
    PCB tmp;
    if(ID>15){
        printf("----------Illegal ID\n");
        return 0;
    }
    //resume the target PCB by poping if from suspendqueue and pushing it into readyqueue
    tmp = suspendQueue->front;
    if(ID==-1&&readyQueue->front==NULL){
       // EnQueueWithPrior(readyQueue, DeQueueWithoutFree(suspendQueue));
    }
    
    
    while (tmp!=NULL ) {
        if(tmp->pid == ID){
           // printf("----------Found the ID\n");
            EnQueueWithPrior(readyQueue, DeleWithoutFree(suspendQueue, tmp));
            return 1;
        }
        tmp = tmp->next;
    }

    return 0;
}

int  ChangePriorByID(long pid, int priority){
    PCB tmp;
    if(priority > 99)
    {
        return 0;
    }
    if(pid == -1 || pid == currentPCB->pid)
    {
        currentPCB->prior = priority;
        return 1;
    }
    // check readyqueue
    tmp = readyQueue->front;
    while (tmp!=NULL)
    {//if the targetPCB is found in this queue  change the priority and resort readyqueue after priority changing
        if(tmp->pid == pid){
            
            tmp->prior = priority;
            DeleWithoutFree(readyQueue, tmp);
            EnQueueWithPrior(readyQueue, tmp);
            return 1;
        }
        tmp = tmp->next;
    }
    //check timerqueue
    tmp = timerQueue->front;
    while (tmp!=NULL)
    {//if the targetPCB is found in this queue  change the priority
        if(tmp->pid == pid){
            tmp->prior = priority;
            
            return 1;
        }
        tmp = tmp->next;
    }
    //check suspendqueue
    tmp = suspendQueue->front;
    while (tmp!=NULL)
    {//if the targetPCB is found in this queue  change the priority
        if(tmp->pid == pid){
            tmp->prior = priority;
            
            return 1;
        }
        tmp = tmp->next;
    }
    
    return 0;
}

int sendMessage(long sid,long tid,char* msg,int msglength){
    //initialize the messageNode
    MSG message;
    if (tid>99) {//invalid target id
        return 0;
    }
    message = (MSG)malloc(sizeof(MSG));
    message->sid = sid;
    message->tid = tid;
    message->length = msglength;
    strcpy(message->msg,msg);
    //push the messagenode into Msgqueue
    EnQueueMsg(msgQueue,message);
    //resume the destination PCB if it's in suspendqueue
    ResumeByID(tid);
    
    return 1;
}

int receiveMessage(long sid, char *msg,int msglength,long *actualLength, long *actualSid){
    MSG tmp;
    if(sid>99){//invalid target id
        //printf("--------------------------------source id is invalid\n");
        return 0;
    }
    memset(msg,'\0',msglength);
    tmp = msgQueue->front;
    while(tmp!=NULL){
        // match the specified source pid and target pid
        if((sid==-1||tmp->sid==sid)&&((tmp->tid==-1&&tmp->sid!=sid)||tmp->tid==currentPCB->pid)){
            if(tmp->length>msglength){//return error if msgbuff is too small to receive the msg
                //printf("------------------------msgbuff is the way too small\n");
                return 0;
            }
            else{
                //destination PCB receives the message and pops the msgnode from msgqueue if everything is fine
                strcpy(msg, tmp->msg);
                // printf("------------------------------the msg is %s\n",msg);
                *actualLength=(long)tmp->length;
                *actualSid = tmp->sid;
                //remove the node from msgQueue
                DeQueueMsg(msgQueue);
                return 1;
            }
        }
        tmp = tmp->next;
    }
    //check if timerquque is empty
    if(timerQueue->front!=NULL){//if null
        while(timerQueue->front!=NULL) {
            CALL(Z502Idle());
            //schedule_printer("Idle", currentPCB->pid);
        }
         EnQueue(readyQueue, currentPCB);
        
    }
    else{//if timerqueue is not null suspend currentPCb into suspendqueue
        EnQueue(suspendQueue,currentPCB);
    }
    
    
    while (readyQueue->front==NULL) {
        if(timerQueue->front==NULL&&readyQueue->front==NULL){
            Z502Halt();
        }
        //idle until readyqueue is no longer empty
        CALL(Z502Idle());
    }
    
    if(readyQueue->front!=NULL){
        currentPCB =  DeQueueWithoutFree(readyQueue);
        //printf("currentPCB=====%ld\n",currentPCB->pid);
    }
    
    Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(currentPCB->context) );
    // recursive, until it finds a message for it
    return receiveMessage(sid, msg, msglength, actualLength, actualSid);
    
}

void schedule_printer(char* actions,INT32 tarGetID){
    if (tarGetID<0) {
        return ;
    }
    
    PCB tmp;
    int count = 0;
    long counter = 655350;
    READ_MODIFY(MEMORY_INTERLOCK_BASE + 11, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
    usleep(10);
     if (printerClt==1) {
         //printf("did i run-------------\n");
         printf("\n");
    SP_setup_action(SP_ACTION_MODE, actions);
    if(currentPCB!=NULL){
        SP_setup(SP_RUNNING_MODE, currentPCB->pid);
    }
    else{
        SP_setup(SP_RUNNING_MODE, 99);
    }
    
    SP_setup(SP_TARGET_MODE, tarGetID);
   
    tmp = readyQueue->front;
    while (tmp!=NULL)
    {
        
        count++;
        SP_setup(SP_READY_MODE, (int)tmp->pid);
        tmp = tmp->next;
        if (count >= 10)// it just print first 10 PCB in this queue
        {
            count = 0;
            break;
        }
    }
    tmp = timerQueue->front;
    while (tmp!=NULL) {
        
        count++;
        SP_setup(SP_TIMER_SUSPENDED_MODE, (int)tmp->pid);
        tmp = tmp->next;
        if (count >= 10)// it just print first 10 PCB in this queue
        {
            count = 0;
            break;
        }
    }
    tmp = suspendQueue->front;
    while (tmp!=NULL)
    {
        
        count++;
        SP_setup(SP_PROCESS_SUSPENDED_MODE, (int)tmp->pid);
        tmp = tmp->next;
        if (count >= 10)// it just print first 10 PCB in this queue
        {
            count = 0;
            break;
        }
    }
   
         SP_print_line();
         printf("\n");
    }
   
    //printf("\n");
    READ_MODIFY(MEMORY_INTERLOCK_BASE + 11, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResultPrinter);
}

void DiskQueuePrinter(DiskQueue *queue){
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
    DISK tmp;
    tmp = queue->front;
    while (tmp!=NULL) {
        
        printf("DISK-%ld sector %ld\n",tmp->diskID,tmp->sectorID);
        tmp= tmp->next;
    }
   // printf("queue size------->%d\n",queue->size);
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
}

void    svc( SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    static short        do_print = 10;
    short               i;
    INT32               Time;
    PCB                 pnode;
    long                pid;
    //start disk variable
    long				disk;
    long				sector;
    INT32               diskStatus;
    char				read_buffer[64];
    char                *data;
    //end
    int                 count=0;
    PCB                 temp;
    INT32               priority;
    int                 result;
    INT32               messageLength;
   static INT32         messageCounter=0;
    char                message[100];
   // INT32               diskStatus;
    
    
    PCB tmp =(PCB)malloc(sizeof(Node));
    
    call_type = (short)SystemCallData->SystemCallNumber;
    if ( do_print > 0 ) {
        
        printf( "SVC handler: %s\n", call_names[call_type]);
        if (printerClt==1) {
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ ){
            //Value = (long)*SystemCallData->Argument[i];
            
                printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
                       (unsigned long )SystemCallData->Argument[i],
                       (unsigned long )SystemCallData->Argument[i]);
            }
            
        }
        do_print--;
    }
    
    switch (call_type) {
        case SYSNUM_GET_TIME_OF_DAY://this vakue is found in syscalls
            CALL(MEM_READ(Z502ClockStatus, &Time));
            *(INT32 *)SystemCallData->Argument[0]= Time;
            
            break;
            
            // terminate system call
            case SYSNUM_TERMINATE_PROCESS:

            if((INT32)SystemCallData->Argument[0] == -2)//terminiate the entire system if paremeter is -2
			{
                *(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
                schedule_printer("Finish",currentPCB->pid);
               
                Z502Halt();
				
                
			}
            else if((INT32)SystemCallData->Argument[0] == -1)//terminiate the running PCB if arg is -1
            {
                READ_MODIFY(MEMORY_INTERLOCK_BASE+3, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
              
                *(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
                
                //check readqueue and timerqueue to terminiate the target PCB
                CALL(TerminateSelf(readyQueue,currentPCB));
                CALL(TerminateSelf(timerQueue,currentPCB));
                
                CALL( schedule_printer("Term",currentPCB->pid));
                // if the both readyqueue and timerqueue are empty just terminiate the entire system
                //if the readyqueue is empty but the timerqueue, keep calling z502idle() until readyqueue is no longer empty
                while(readyQueue->front==NULL){
                    if(timerQueue->front==NULL){
                        Z502Halt();
                    }
                    CALL(Z502Idle());
                    
                    CALL(schedule_printer("Idle", currentPCB->pid));
                }
                //pop the first PCB from readyqueue and run it
                if(!IsEmpty(readyQueue)){
                    CALL(currentPCB = DeQueueWithoutFree(readyQueue));
                    temp = currentPCB;
                }
                
                READ_MODIFY(MEMORY_INTERLOCK_BASE+3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
               CALL (Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(temp->context)));
                
            }
            else// if the paremeter neither -1 or -2
            {
                
                tmp = readyQueue->front;
                
                while (tmp!=NULL) {
                    if (tmp->pid == (long)SystemCallData->Argument[0]) {
                        CALL(TerminateSelf(readyQueue,tmp));
                       // DeQueue(readyQueue);
                        *(long *)SystemCallData->Argument[1] =ERR_SUCCESS;
                        currentPCBnum--;
                    }
                    tmp = tmp->next;
                }
            }
            
            break;
            
            //sleep system call
            case SYSNUM_SLEEP:
        
            start_timer(SystemCallData->Argument[0]);
      
            break;
            
            case SYSNUM_CREATE_PROCESS:
            
            pnode = (PCB)malloc(sizeof(Node));
            //initialize the PCB that the system is going to create
            InitPCB2(SystemCallData, pnode);
            //create the PCB and push it into readyqueue
            pid = CreateProcess(pnode);
            
            if (pid==ILLEGAL_PRIORITY) {
                printf("ILLEGAL_PRIORITY\n\n");
                *(long *)SystemCallData->Argument[3] = pid;
                *(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
            }
            else if(pid==DUPLICATED){// if the PCB name is duplicated drop the duplicated PCB and return a error
                printf("DUPLICATED NAME\n\n");
                *(long *)SystemCallData->Argument[3] = pid;
                *(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
            }
            else {
                *(long *)SystemCallData->Argument[3] = pid;
                *(long *)SystemCallData->Argument[4] = ERR_SUCCESS;
                schedule_printer("Create", pid);
            }
            
            if(currentPCBnum > 15)// i define the limitation of number of PCB is 15 that means if the number of PCB is beyonds 15 the system will stop creating new PCB and return a error
			{
                printf("you can't create PCB any more since the limitation is 15\n");
				*(long *)SystemCallData->Argument[4] = ERR_BAD_DEVICE_ID;
			}
            
            
           
            break;
            
        case SYSNUM_GET_PROCESS_ID:
            pid = GetIDByName((char*)SystemCallData->Argument[0]);
            //printf("==========%ld\n",pid);
            if (pid<15) {
                *(long *)SystemCallData->Argument[1] = pid;

                *(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
                //printf("\n=--------found ID\n");
            }
            else{
                *(long *)SystemCallData->Argument[1] = pid;
                
                *(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
                //printf("\n can not find %s\n",(char*)SystemCallData->Argument[0]);
            }
            
            
            
            break;
            
        case SYSNUM_SUSPEND_PROCESS:
            printf("suspend called\n");
            
            pid =(long)SystemCallData->Argument[0];
			//returnStatus = suspend_by_PID(pid);
            
            //SuspendByID(pid);
			if(SuspendByID(pid))
			{
                if (SystemCallData->Argument[0]!=-1) {//if the pid is -1 the printer will not print the schedule
                    schedule_printer("suspend",SystemCallData->Argument[0]);
                }
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
   
			
			
            break;
            
            case SYSNUM_RESUME_PROCESS:
            
            printf("resume called\n");
          
            pid =(long)SystemCallData->Argument[0];
            //ResumeByID(pid);
            if(ResumeByID(pid))
			{
                schedule_printer("resume",SystemCallData->Argument[0]);
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}

            break;
            
            case SYSNUM_CHANGE_PRIORITY:
            
            //printf("-------------iam here");
            pid = (long)SystemCallData->Argument[0];
            priority = (int)SystemCallData->Argument[1];
            
            result = ChangePriorByID(pid, priority);
            if(result == 1)
			{
                schedule_printer("ch_prior",pid);
				*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			else
			{
				*(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
            
            
            break;
            
        case SYSNUM_SEND_MESSAGE:
            messageCounter++;
            pid = (long)SystemCallData->Argument[0];
            messageLength =(INT32)SystemCallData->Argument[2];
            //printf("this is  msgcounter%d\n",messageCounter);
            //sendMessage();
            if (messageCounter>MAX_COUNT) {//stop sending new message if the number of message reach to limitation
                //printf("i am here-----\n");
                messageCounter=0;
                *(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
				break;
            }
            if(messageLength>MAX_LENGTH){//drop the message if the length of message reach to limitation
                
                *(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
				break;
            }
            else{
                
                strcpy(message,(char*)SystemCallData->Argument[1]);
                //send the message and check result
                if(sendMessage(currentPCB->pid,pid,message,messageLength))
				{
					*(long *)SystemCallData->Argument[3] = ERR_SUCCESS;
                    // schedule_printer("send", (INT32)SystemCallData->Argument[0]);
				}
				else
				{
                  
					*(long *)SystemCallData->Argument[3] = ERR_BAD_PARAM;
				}
            }

            break;
            
        case SYSNUM_RECEIVE_MESSAGE:
            pid = (long)SystemCallData->Argument[0];
            
            messageLength =(INT32)SystemCallData->Argument[2];
            
            if(messageLength>MAX_LENGTH){//invalid message length
                printf("---------------------beyonged the max length\n");
                *(long *)SystemCallData->Argument[5] = ERR_BAD_PARAM;
            }
            else{//receive the messaga and check the result
                if(receiveMessage(pid, (char*)SystemCallData->Argument[1], messageLength, SystemCallData->Argument[3], SystemCallData->Argument[4])){
                    *(long *)SystemCallData->Argument[5] = ERR_SUCCESS;
                
                }else{
                    *(long *)SystemCallData->Argument[5] = ERR_BAD_PARAM;
                }
                    
            }
            break;
            
        case SYSNUM_DISK_READ:
            
           
            disk = (long)SystemCallData->Argument[0];
            sector = (long)SystemCallData->Argument[1];
            //printf("----------ID1 %ld,ID2 %ld----------\n",diskID,sectorID);
            DISKreadOrWrite(disk,sector,read_buffer,DISK_READ);
          
            
            memcpy (SystemCallData->Argument[2], read_buffer, PGSIZE);
          
            
           // printf("iam here read-----%s\n\n",data);
        
            
            
            break;
            
            case SYSNUM_DISK_WRITE:
            //test=(char *)SystemCallData->Argument[2];
            disk = (long)SystemCallData->Argument[0];
            sector = (long)SystemCallData->Argument[1];
            data = (char *)SystemCallData->Argument[2];
            //printf("----------ID1 %ld,ID2 %ld----------\n",diskID,sectorID);
            //memcpy (SystemCallData->Argument[2], data, PGSIZE);
            //test=(char *)SystemCallData->Argument[2];
            
            //printf("wtf2-----------\n");
            
           // memcpy(buffer, SystemCallData->Argument[2], PGSIZE);
            //printf("iam here write-----%s\n\n",buffer);
            DISKreadOrWrite(disk,sector,data,DISK_WRITE);
            
            
            
            
            break;
            

            
            
        default:
            printf("Error! call_type not recognized!\n");
            printf("call_type is %i\n",call_type);
            break;
    }
}                                               // End of svc



/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;
    int                 l;
    //initialize queues that will be used in my system
    initFrame();
    readyQueue = InitQueue();
    timerQueue = InitQueue();
    suspendQueue = InitQueue();
    msgQueue = InitMsgQueue();
    for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
         diskqueue[l] = InitDiskQueue();
    }
    diskStack = InitDiskStack();
    /* Demonstrates how calling arguments are passed thru to here       */
    
    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );
    
    /*          Setup so handlers will come to code in base.c           */
    
    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;
    
    /*  Determine if the switch was set, and if so go to demo routine.  */
    
    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1a" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1a, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1b" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1b, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1c" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1c, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1d" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1d, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "test0" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test0, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1e" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1e, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1f" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1f, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1m" ) == 0 ) ){
        printerClt=0;
        Z502MakeContext( &next_context, (void *)test1m, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1g" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1g, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1h" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1h, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1j" ) == 0 ) ){
        printerClt=0;
        Z502MakeContext( &next_context, (void *)test1j, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1i" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1i, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "1l" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1l, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "1k" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test1k, USER_MODE );
        
    }else if(( argc > 1 ) && ( strcmp( argv[1], "test2a" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test2a, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2b" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test2b, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2c" ) == 0 ) ){
        //printerClt=0;
        Z502MakeContext( &next_context, (void *)test2c, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2d" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test2d, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2g" ) == 0 ) ){
        
        Z502MakeContext( &next_context, (void *)test2g, USER_MODE );
        
    }
    //Initial the first PCB
    //PCB temp =(PCB)malloc(sizeof(Node));
    currentPCB = (PCB)malloc(sizeof(Node));
    startPCB = (PCB)malloc(sizeof(Node));
    startPCB-> pid = 0;
    startPCB->context = next_context;
    startPCB->prior = 1;
    startPCB->next = NULL;
    strcpy(startPCB->name, "StartPCD");
    currentPCB = startPCB;
    
    
    /* This routine should never return!!           */
    
    /*  This should be done by a "os_make_process" routine, so that
     test0 runs on a process recognized by the operating system.    */
    Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &(currentPCB->context) );
}                                               // End of osInit