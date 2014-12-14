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
static int counter=0;
int Test2d=0;
int modelctrl=1;



UINT16 *PAGE_HEAD[10];
FRAME frame[PHYS_MEM_PGS];
DiskQueue *diskqueue[MAX_NUMBER_OF_DISKS];
Shadow_Page_Table shadow_page_table[MAX_NUMBER_OF_DISKS][VIRTUAL_MEM_PAGES];



extern char MEMORY[PHYS_MEM_PGS * PGSIZE ];
INT32 static victim=0;
INT32 static realvic=0;

long CreateProcess(PCB pnode);
INT32 GetIDByName(char* name);
int sendMessage(long sid,long tid,char* msg,int msglength);
int receiveMessage(long sid, char *msg,int msglength,long *actualLength, long *actualSid);
void schedule_printer();
void QueuePrinter(Queue *queue);
int  ChangePriorByID(long pid, int priority);
INT32  SuspendByID(long ID);
INT32  ResumeByID(long ID);

//phase 2
void initFrame();
void shadow_page_table_Init();
void DISKreadOrWrite(long diskID, long sectorID, char* buffer, int readOrWrite);
void DiskQueuePrinter(DiskQueue *queue);
void transfer(INT32 ID);
void FIFO(long status);
void sec_chance(long status);
void mapping();
void findVictim();
/************************************************************************
 This fucntion used to find next victim, it is called in second chance page
 replacement algorithm
 ************************************************************************/
void findVictim(){
    while (victim<63) {
        //sleep(1);
        //printf("----------------------------%x=?%x\n",Z502_PAGE_TBL_ADDR[frame[victim].pageID]&PTBL_REFERENCED_BIT,PTBL_REFERENCED_BIT);
        if ((UINT16)Z502_PAGE_TBL_ADDR[frame[victim].pageID]&PTBL_REFERENCED_BIT!= PTBL_REFERENCED_BIT ) {//check the reference bit.
            //printf("found it\n");
            break;
        }
        realvic++;
        if(realvic>63){
            realvic=realvic%PHYS_MEM_PGS;
            
        }
        victim=realvic;
    }
}
/************************************************************************
 This function used to set up a shadow_page_table by using frame table
 information
 input void
 output void
 ************************************************************************/
void mapping(){
    shadow_page_table[frame[victim].pid][frame[victim].pageID].disk =(INT32)frame[victim].pid+1;
    shadow_page_table[frame[victim].pid][frame[victim].pageID].sector=(INT32)frame[victim].pageID;
    shadow_page_table[frame[victim].pid][frame[victim].pageID].page=(INT32)frame[victim].pageID;
    shadow_page_table[frame[victim].pid][frame[victim].pageID].isAvailable=FALSE;

}
/************************************************************************
 This function implement second chance page replacement algorithm this 
 funciton will be called in fault_handler
 input status
 output void
 ************************************************************************/
void sec_chance(long status){
    INT32       Frame;
    victim=realvic;
    //find next victim
    findVictim();
    Frame=(INT32)frame[victim].frameID;
            
    PAGE_HEAD[frame[victim].pid][frame[victim].pageID] = 64;
    PAGE_HEAD[frame[victim].pid][frame[victim].pageID] &= 0x7FFF;
    //setup a shadow page table
    mapping();
    //write old page inforamtion to disk
    DISKreadOrWrite((long)(frame[victim].pid+1),
                    (long)frame[victim].pageID,
                    (char*)&MEMORY[Frame*PGSIZE],
                    DISK_WRITE);
    
    if (!shadow_page_table[currentPCB->pid][status].isAvailable)
    {   //read the page information from disk
        DISKreadOrWrite(shadow_page_table[currentPCB->pid][status].disk,
                        shadow_page_table[currentPCB->pid][status].sector,
                        (char*)&MEMORY[Frame* PGSIZE],
                        DISK_READ);
        //reset the shadow page table
        shadow_page_table[currentPCB->pid][status].isAvailable = TRUE;
        shadow_page_table[currentPCB->pid][status].page = status;
        
    }
    
    Z502_PAGE_TBL_ADDR[status] = (UINT16)Frame|0x8000;
    frame[victim].pageID = status;
    frame[victim].pid = currentPCB->pid;
   
}
/************************************************************************
 This function implement FIFO page replacement algorithm, this funciton 
 will be called in fault_handler
 input status
 output void
 ************************************************************************/
void FIFO(long status){
    INT32       Frame;
    Frame=(INT32)frame[victim].frameID;
    
    PAGE_HEAD[frame[victim].pid][frame[victim].pageID] &= 0x7FFF;
    //setup a shadow page table
    mapping();
    //write old page inforamtion to disk
    DISKreadOrWrite((long)(frame[victim].pid+1),
                    (long)frame[victim].pageID,
                    (char*)&MEMORY[Frame*PGSIZE],
                    DISK_WRITE);
    
    if (!shadow_page_table[currentPCB->pid][status].isAvailable)
    {   //read the page information from disk
        
        DISKreadOrWrite(shadow_page_table[currentPCB->pid][status].disk,
                        shadow_page_table[currentPCB->pid][status].sector,
                        (char*)&MEMORY[Frame*PGSIZE],
                        DISK_READ);
        //reset the shadow page table
        shadow_page_table[currentPCB->pid][status].isAvailable = TRUE;
        shadow_page_table[currentPCB->pid][status].page = status;
        
    }
    
    Z502_PAGE_TBL_ADDR[status] = (UINT16)Frame|PTBL_VALID_BIT;
    frame[victim].pageID = status;
    frame[victim].pid = currentPCB->pid;
    //find next victim page
    victim++;
    if(victim>63){
        victim=victim%PHYS_MEM_PGS;
    }

}
/************************************************************************
 This funciton used to check if the disk has finished the I/O 
 if it has finished the I/O, it will return the relevant PCBs which are 
 in diskqueue to readyqueue
 input DiskID
 output void
 ************************************************************************/

void transfer(INT32 ID){
    int Status;
    DISK tmp;
    DISK tmp2;
    //check disk status
    CALL(MEM_WRITE(Z502DiskSetID, &ID));
    CALL(MEM_READ(Z502DiskStatus, &Status));
    READ_MODIFY(MEMORY_INTERLOCK_BASE+7, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
    // if this disk is not in use, check disk queue
    if(Status==DEVICE_FREE){
        tmp=diskqueue[ID]->front;
       // printf("test where is the fking ---------\n");
        //tmp2=diskqueue[ID]->front->next;
        while (tmp!=NULL&&tmp->GetDisk==1) {
            
            EnQueueWithPrior(readyQueue, tmp->PCB);
            //printf("i am %ld pushed into readyqueue----------\n",tmp->PCB->pid);
            //schedule_printer("trans", tmp->PCB->pid);
            //for(int i=1;i<4;i++){
            //DiskQueuePrinter(diskqueue[i]);
            //}
            tmp=tmp->next;
            //tmp2=tmp2->next;
        }
        diskqueue[ID]->front=tmp;
        if(diskqueue[ID]->front!=NULL){
            EnQueueWithPrior(readyQueue, diskqueue[ID]->front->PCB);
            diskqueue[ID]->front=diskqueue[ID]->front->next;
        }
        //usleep(100);
       
    }
    READ_MODIFY(MEMORY_INTERLOCK_BASE+7, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}


/************************************************************************
 This function used to write data to disk or read data back from disk.
 input: diskID sectorID buffer(data) readOrwrite
 if action=1 read data from disk
 if action=0 write data to disk
 output void
 ************************************************************************/
void DISKreadOrWrite(long diskID, long sectorID, char* buffer, int action){
    int Status;

    int i=1;
    //check the status of this disk
    MEM_WRITE(Z502DiskSetID, &diskID);
    MEM_READ(Z502DiskStatus, &Status);
     usleep(50);
     //printf("---------------------\n");
        while(Status==DEVICE_IN_USE) {
            //printf("i ama the %ld----\n",diskID);
              EnQueueDisk(diskqueue[(INT32)diskID], InitDisk(currentPCB,0));
            
            while(readyQueue->front==NULL){
                int i;
                
                for(i=1;i<MAX_NUMBER_OF_DISKS;i++){
                    transfer(i);
                }
            }
            currentPCB=DeQueueWithoutFree(readyQueue);
            schedule_printer("re_PCB", currentPCB->pid);
            for(i=1;i<MAX_NUMBER_OF_DISKS;i++){
                DiskQueuePrinter(diskqueue[i]);
            }
            Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(currentPCB->context));
            CALL(MEM_WRITE(Z502DiskSetID, &diskID));
            CALL(MEM_READ(Z502DiskStatus, &Status));
            
        }
    
   //
    //printf("---------------------\n");
    if (action==DISK_WRITE) {
    //setup diskID secterID and buffer and action
    CALL(MEM_WRITE(Z502DiskSetID, &diskID));
        
    //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
    CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
    CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
   
    CALL(MEM_WRITE(Z502DiskSetAction, &action));
    Status = 0;
    //start the action
    CALL(MEM_WRITE(Z502DiskStart, &Status));
    }

    if(action==DISK_READ){
    CALL(MEM_WRITE(Z502DiskSetID, &diskID));
  
    CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
    CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
    
   
    CALL(MEM_WRITE(Z502DiskSetAction, &action));
    Status = 0;
    CALL(MEM_WRITE(Z502DiskStart, &Status));
         //schedule_printer("disk_read", currentPCB->pid);
    
    }
    
    
    //check the status of this disk
    MEM_WRITE(Z502DiskSetID, &diskID);
    MEM_READ(Z502DiskStatus, &Status);
 
    EnQueueDiskHead(diskqueue[(INT32)diskID], InitDisk(currentPCB,1));
    while(readyQueue->front==NULL){
        int i;
        
        for(i=1;i<MAX_NUMBER_OF_DISKS;i++){
            transfer(i);
        }
    }
    currentPCB=DeQueueWithoutFree(readyQueue);
    schedule_printer("re_PCB", currentPCB->pid);
    for(i=1;i<4;i++){
        DiskQueuePrinter(diskqueue[i]);
    }
    Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(currentPCB->context));

    
    //
   
    //EnQueueDisk(diskqueue[(INT32)diskID], InitDisk(1,sectorID, readOrWrite,currentPCB,0));
    //EnQueueDiskHead(diskqueue[(INT32)diskID], InitDisk(2,sectorID, readOrWrite,currentPCB,1));
    //for(int i=1;i<4;i++){
       // DiskQueuePrinter(diskqueue[1]);
    //}
     
   
    /*if (readOrWrite==DISK_WRITE) {
        //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
        CALL(MEM_WRITE(Z502DiskSetID, &diskID));
        
        //printf("----------ID1 %ld,ID2 %ld-readorwrite %d---------\n",diskID,sectorID,readOrWrite);
        CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
        CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
        
        CALL(MEM_WRITE(Z502DiskSetAction, &readOrWrite));
        diskStatus = 0;
        CALL(MEM_WRITE(Z502DiskStart, &diskStatus));
        MEM_WRITE(Z502DiskSetID, &diskID);
        
        MEM_READ(Z502DiskStatus, &diskStatus);
        printf("-----------------------------------wo kai shi le a \n");
        while(diskStatus==DEVICE_IN_USE){
            printf("-----------------------------------wo zaijixu \n");
            for(int i=1;i<30;i++){
                CALL();;
            }
            MEM_WRITE(Z502DiskSetID, &diskID);
            
            MEM_READ(Z502DiskStatus, &diskStatus);
        }
    }
    
    //usleep(1000);
    
    
    //
    if(readOrWrite==DISK_READ){
        CALL(MEM_WRITE(Z502DiskSetID, &diskID));
        
        CALL(MEM_WRITE(Z502DiskSetSector, &sectorID));
        CALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)buffer));
        
        
        CALL(MEM_WRITE(Z502DiskSetAction, &readOrWrite));
        diskStatus = 0;
        CALL(MEM_WRITE(Z502DiskStart, &diskStatus));
        printf("-----------------------------------wo kai shi le a \n");
        MEM_WRITE(Z502DiskSetID, &diskID);
        
        MEM_READ(Z502DiskStatus, &diskStatus);
        while(diskStatus==DEVICE_IN_USE){
            printf("-----------------------------------wo zaijixu \n");
            for(int i=1;i<30;i++){
                CALL();
            }
            MEM_WRITE(Z502DiskSetID, &diskID);
            
            MEM_READ(Z502DiskStatus, &diskStatus);
        }
        
    }
    //usleep(1000);*/
}

void memory_printer()
{
    int i = 0;
    
    if (counter % modelctrl == 0){
        for (i = 0; i < PHYS_MEM_PGS; i++)
        {      
              //  Frame is Valid - the physical frame contains real data:               4
              //  Frame is Modified - some process has written to it and it is dirty:   2
              //  Frame is Referenced - some process has written or read it:            1
                
                MP_setup(frame[i].frameID,frame[i].pid,frame[i].pageID,
                         ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_VALID_BIT) >> 13)+
                         ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_MODIFIED_BIT) >> 13) +
                         ((Z502_PAGE_TBL_ADDR[frame[i].pageID] & PTBL_REFERENCED_BIT) >> 13));
            
        }
        
        MP_print_line();
        printf("\n");
        
    }
    counter++;
}
/************************************************************************
 this function used to initialize frame page table
 ************************************************************************/
void initFrame(){
    int i;
    for (i= 0; i < PHYS_MEM_PGS; i++)
    {
        frame[i].frameID = i;
        frame[i].occupied = FALSE;
        frame[i].pageID = -1;
    }
}
/************************************************************************
 this function used to initialize shadow page table
 ************************************************************************/
void shadow_page_table_Init()
{
    int i = 0;
    int j = 0;
    for(i = 0; i < MAX_NUMBER_OF_DISKS; i++)
    {
        for(j = 0; j < VIRTUAL_MEM_PAGES; j++)
        {
            shadow_page_table[i][j].disk = NULL;
            shadow_page_table[i][j].page = NULL;
            shadow_page_table[i][j].isAvailable = TRUE;
            
        }
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
    int                 i;
    //printf("i am the test2========\n");
    //printf("i am here----------\n");
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    if (counter % modelctrl == 0){
    printf( "Interrupt handler: Found device ID %d with status %d\n",device_id, status );
    }
    
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
       READ_MODIFY(MEMORY_INTERLOCK_BASE+10,DO_LOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
       for(i =1;i<MAX_NUMBER_OF_DISKS;i++){
           if(device_id-4!=i){
               transfer(i);
           }
       }
       READ_MODIFY(MEMORY_INTERLOCK_BASE+10,DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &TimeLockResult);
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
    INT32       page;
    INT32       Frame;
    
    INT32       disk;
    INT32       sector;
    static BOOL  alltoken = FALSE;
    int             i;
    
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    if (counter % modelctrl == 0){
    printf( "Fault_handler: Found vector type %d with value %d\n",
           device_id, status );
    }
    
    if(device_id == SOFTWARE_TRAP){//device_id=0
        CALL(Z502Halt());
    }
    else if(device_id == CPU_ERROR){//device_id=1
        CALL(Z502Halt());
    }
    else if(device_id == INVALID_MEMORY){//device_id=2
        
        
        if (status >= VIRTUAL_MEM_PAGES){ //Address is larger than page table,
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
        
        if (!alltoken) {
            for (i=0; i<PHYS_MEM_PGS; i++) {
                if(!frame[i].occupied){
                    //set up a frame for the page
                    Z502_PAGE_TBL_ADDR[status] = (UINT16)frame[i].frameID | PTBL_VALID_BIT;
                    frame[i].occupied=TRUE;
                    frame[i].pageID = status;
                    frame[i].pid = currentPCB->pid;
                    
                   // printf("frame[%ld]  page %ld\n",frame[i].frameID
                    //       ,frame[i].pageID);
                    break;
                }
                
            }
            if (i==PHYS_MEM_PGS) {//all page have been occupied
                alltoken = TRUE;
            }
            
        }
        if(alltoken){//if all page have been used, use page replacement algorithm to replace old page with a
                     //new page
            
            //FIFO(status);//fifo page replacement algorithm
            sec_chance(status);//second chance replacement algorithm

        }
       // sleep(1);
    }
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
    printf("SleepTime: %d\n",(INT32)SleepTime);
   
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
/************************************************************************
 for testing, not use in my final program
 ************************************************************************/
void DiskQueuePrinter(DiskQueue *queue){
    if(Test2d==1){
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
    DISK tmp;
    tmp = queue->front;
    while (tmp!=NULL) {
        
        printf("PCB%ld(%d)",tmp->PCB->pid,tmp->GetDisk);
        tmp= tmp->next;
    }
    printf("\n");
    CALL(READ_MODIFY(MEMORY_INTERLOCK_BASE+1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult));
    }
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
        for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
            diskqueue[l] = InitDiskQueue();
        }
        //printerClt=0;
        Z502MakeContext( &next_context, (void *)test2c, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2d" ) == 0 ) ){
        for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
            diskqueue[l] = InitDiskQueue();
        }
        Test2d=1;
        Z502MakeContext( &next_context, (void *)test2d, USER_MODE );
        
    }
   
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2e" ) == 0 ) ){
        for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
            diskqueue[l] = InitDiskQueue();
        }
        modelctrl=10;
        shadow_page_table_Init();
        Z502MakeContext( &next_context, (void *)test2e, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2f" ) == 0 ) ){
        for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
            diskqueue[l] = InitDiskQueue();
        }
        printerClt=0;
        modelctrl=100;
        shadow_page_table_Init();
        Z502MakeContext( &next_context, (void *)test2f, USER_MODE );
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2g" ) == 0 ) ){
        /*for (l=0; l<MAX_NUMBER_OF_DISKS; l++) {
            diskqueue[l] = InitDiskQueue();
        }
        shadow_page_table_Init();
        Z502MakeContext( &next_context, (void *)test2g, USER_MODE );*/
        printf("****************************************\n\n");
        printf("sorry i tried, but i failed !!!!\n\n");
        printf("****************************************\n\n");
        exit(0);
        
    }
    else if(( argc > 1 ) && ( strcmp( argv[1], "test2h" ) == 0 ) ){
        printf("****************************************\n\n");
        printf("sorry i did not implement this test!!!!1\n\n");
        printf("****************************************\n\n");
        exit(0);
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