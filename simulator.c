#include "simulator.h"
#include <string.h>

typedef struct pageTable{
    int pageLevel;
    int pageIndex;
    int pageSize;
}pageTable;

/*
typedef struct TLBnode{
    //int* v;
    //char* tag;
    unsigned int tag;
    //unsigned int *pte;
}TLBnode;
*/


typedef struct ppNode{
    int valid;
    unsigned int VPN;
    gll_t* vphead;
}ppNode;


typedef struct PNode{
    unsigned int *vp;
}PNode;


typedef struct VP{
    char v;
    void *next_addr;
}VP;

// helper func power
int power(int power){
    int i;
    int base = 1; 
    for(i=0; i<power; i++){
      base*=2;
    }
    return base;
}

void init()
{
    printf("func init start\n"); //test
    current_time = 0;
    nextQuanta = current_time + quantum;
    readyProcess = gll_init();
    runningProcess= gll_init();
    blockedProcess = gll_init();

    processList = gll_init();

    traceptr = openTrace(traceFileName);
    sysParam = readSysParam(traceptr);
    
    //read traces from trace file and put them in the processList
    struct PCB* temp = readNextTrace(traceptr);
    if(temp == NULL)
    {
        printf("No data in file. Exit.\n");
        exit(1);
    }

    while(temp != NULL)
    {
        gll_pushBack(processList, temp);
        temp = readNextTrace(traceptr);
    }
    
    //transfer ready processes from processList to readyProcess list
    temp = gll_first(processList);
    
    while((temp!= NULL) && ( temp->start_time <= current_time))
    {
        struct NextMem* tempAddr;
        temp->memoryFile = openTrace(temp->memoryFilename);
        temp->numOfIns = readNumIns(temp->memoryFile);
        tempAddr = readNextMem(temp->memoryFile);
        while(tempAddr!= NULL)
        {
            gll_pushBack(temp->memReq, tempAddr);
            tempAddr = readNextMem(temp->memoryFile);
        }
        
        //write from here->
        /*
        gll_t* VPList = gll_init();
        int i;
        for(i=0; i<power(sysParam->N1_in_bits); i++){
            struct VP *vp = malloc(sizeof(struct VP));
            vp->v = 0;
            vp->next_addr = NULL;
            gll_push(VPList, vp);
            
        }
        temp->vphead = VPList;
        printf("vpList: %p\n",temp->vphead);
        */
        //<-to here
        
        gll_pushBack(readyProcess, temp);
        gll_pop(processList);

        temp = gll_first(processList);
    }
    //TODO: Initialize what you need
    
    TLBList = gll_init();
    
    PPTList = gll_init();
    int i;
    //init TLB
    for(i=0; i<(sysParam->TLB_size_in_entries); i++){
        unsigned int* tlb = calloc(1, sizeof(unsigned int));
        //int iv = 0;
        //tlb->v = &iv;
        gll_push(TLBList, tlb);
    }
    
    int ppn = (power(22)) / (power(sysParam->P_in_bits)); // ppn
    // init pp
    for (i=0; i<ppn; i++){
        struct ppNode *ppt = malloc(sizeof(struct ppNode));
        ppt->valid = 0;
        gll_push(PPTList, ppt);
    }
    
    disk_interrupt_clock = 0;
    
}

void finishAll()
{
    if((gll_first(readyProcess)!= NULL) || (gll_first(runningProcess)!= NULL) || (gll_first(blockedProcess)!= NULL) || (gll_first(processList)!= NULL))
    {
        printf("Something is still pending\n");
    }
    gll_destroy(readyProcess);
    gll_destroy(runningProcess);
    gll_destroy(blockedProcess);
    gll_destroy(processList);
    
    printf("func finishAll end\n"); // test
//TODO: Anything else you want to destroy
    gll_destroy(TLBList);
    closeTrace(traceptr);
}

void statsinit()
{
    // statsList = gll_init();
    resultStats.perProcessStats = gll_init();
    resultStats.executionOrder = gll_init();
    resultStats.start_time = current_time;
    
}

void statsUpdate()
{
    resultStats.OSModetime = OSTime;
    resultStats.userModeTime  = userTime;   
    resultStats.numberOfContextSwitch = numberContextSwitch;
    resultStats.end_time = current_time;
}

//returns 1 on success, 0 if trace ends, -1 if page fault
int readPage(struct PCB* p, uint64_t stopTime)
{   printf("func readP start\n"); //test
    struct NextMem* addr = gll_first(p->memReq);
    uint64_t timeAvailable = stopTime - current_time;
    
    if(addr == NULL)
    {
        return 0;
    }
    if(debug == 1)
    {
        printf("Request::%s::%s::\n", addr->type, addr->address);
    }

    if(strcmp(addr->type, "NONMEM") == 0)
    {
        uint64_t timeNeeded = (p->fracLeft > 0)? p->fracLeft: sysParam->non_mem_inst_length;
    
        if(timeAvailable < timeNeeded)
        {
            current_time += timeAvailable;
            userTime += timeAvailable;
            p->user_time += timeAvailable;
            p->fracLeft = timeNeeded - timeAvailable;
        }
        else{
            gll_pop(p->memReq);
            current_time += timeNeeded; 
            userTime += timeNeeded;
            p->user_time += timeNeeded;
            p->fracLeft = 0;
        }

        if(gll_first(p->memReq) == NULL)
        {
            return 0;
        }
        return 1;
    }
    else{
        //TODO: for MEM traces
        //unsigned int TAG;  // tlb tag
        p->numOfIns++;
        char TLBfound = 0;
        int i;
        
        unsigned int addr_tag = (unsigned int)strtol(addr -> address, NULL, 16) >> sysParam->P_in_bits;
        
        for(i=0; i<16; i++){
        //printf("%d\n",i);
        //printf("tlbnode: %u\n", *(unsigned int*)gll_get(TLBList, i));
        //printf("addr_tag: %u\n", addr_tag);
        //printf("b\n");
        //printf("if statement: %d", ((unsigned int)(((TLBnode*)gll_get(TLBList, i))->tag, NULL, 16) == addr_tag));
        
            if((*(unsigned int*)gll_get(TLBList, i)) == addr_tag){
                //struct TLBnode* tlbn = gll_get(TLBList, i);
                TLBfound = 1;
                //TAG = (unsigned int)strtol(gll_get(TLBList, i)->tag);  // convert char tag into unsigned int tag
                break;
            }
        }
        addr_tag = (unsigned int) strtol(addr->address, NULL, 16);
        unsigned int l1 = addr_tag >> (sysParam->N2_in_bits + sysParam->N3_in_bits);
        unsigned int l2 = addr_tag << (32 - sysParam->N2_in_bits - sysParam->N3_in_bits);
        l2 = l2 >> (32 - sysParam->N2_in_bits);
        unsigned int l3 = addr_tag << (32 - sysParam->N3_in_bits);
        l3 = l3 >> (32 - sysParam->N3_in_bits);
            
        
        
        timeAvailable -= sysParam->TLB_latency;
        //int v = 1;
        if (TLBfound == 1){  
            //TLB hit
            if(timeAvailable - sysParam->DRAM_latency > 0){    //if time is enough, access DRAM
                
                timeAvailable -= sysParam->DRAM_latency;
                
                //update TLB .fin
                unsigned int* temp = gll_get(TLBList, i);
                //temp->v = &v;
                gll_remove(TLBList, i);
                gll_pushBack(TLBList, temp);
                
                
                p->hitCount++;
                
                
                //update PPT
                struct VP* vp = (VP*)(gll_get(((VP*)gll_get(((VP*)gll_get(p->vphead, l1))->next_addr, l2))->next_addr,l3));
                struct ppNode* ppn = vp->next_addr;
                for(i=0; i<(power(22)) / (power(sysParam->P_in_bits)); i++){
                    if(((ppNode*)gll_get(PPTList, i)) == ppn){
                        break;
                    }
                }
                gll_remove(PPTList, i);
                gll_pushBack(PPTList, ppn);
                
                // pop instruction
                gll_pop(p->memReq);
                /*
                struct ppNode* tempp = gll_first(PPTList);
                gll_pop(PPTList);
                tempp->pAddr = p;
                tempp->addr = addr;
                gll_pushBack(PPTList, tempp);
                
                */
                printf("rp: 1\n");
                return 1;
            }
            else{    //time not enough, exit
                gll_pushBack(blockedProcess, gll_first(runningProcess));
                gll_pop(runningProcess);
                printf("rp: 2\n");  //test
                return 1;
            }
        }
        // TLB miss
        p->missCount++;
        if(timeAvailable - sysParam->DRAM_latency > 0){    //TLB miss, 
            // slice tag into l1, l2, and l3
            
            /*unsigned int address = (unsigned int) strtol(addr->address, NULL, 16);
            unsigned int l1 = address >> (sysParam->N2_in_bits + sysParam->N3_in_bits);
            unsigned int l2 = address << (32 - sysParam->N2_in_bits - sysParam->N3_in_bits);
            l2 = l2 >> (32 - sysParam->N2_in_bits);
            unsigned int l3 = address << (32 - sysParam->N3_in_bits);
            l3 = l3 >> (32 - sysParam->N3_in_bits);
            */
            
            if((p->vphead == NULL)||((((VP*)gll_get(p->vphead, l1))->v) == 0)||((((VP*)gll_get(((VP*)gll_get(p->vphead, l1))->next_addr, l2))->v) == 0)){
              // page fault
              current_time += sysParam->Page_fault_trap_handling_time;
              nextQuanta += sysParam->Page_fault_trap_handling_time;
              OSTime += sysParam->Page_fault_trap_handling_time;
              printf("rp: return -1 \n");  //test
              
              gll_pushBack(blockedProcess, gll_first(runningProcess));
              gll_pop(runningProcess);
              
              return -1;
            }
            // page hit, update TLB
            timeAvailable -= sysParam->DRAM_latency;
            
            //update TLB .fin
            unsigned int* temp = gll_pop(TLBList);
            *temp = addr_tag;
            gll_pushBack(TLBList, temp);
            
            //update PPT
            struct VP* vp = (VP*)(gll_get(((VP*)gll_get(((VP*)gll_get(p->vphead, l1))->next_addr, l2))->next_addr,l3));
            struct ppNode* ppn = vp->next_addr;
            for(i=0; i<(power(22)) / (power(sysParam->P_in_bits)); i++){
                if(((ppNode*)gll_get(PPTList, i)) == ppn){
                    break;
                }
            }
            gll_remove(PPTList, i);
            gll_pushBack(PPTList, ppn);
            
            /*
            struct ppNode* tempp = gll_first(PPTList);
            gll_pop(PPTList);
            tempp->pAddr = p;
            tempp->addr = addr;
            gll_pushBack(PPTList, tempp);
            
            */
            gll_pop(p->memReq);
            printf("rp: 3\n");  //test
            return 1;
                
        }
        else{    //time not enough, exit
            gll_pushBack(blockedProcess, gll_first(runningProcess));
            gll_pop(runningProcess);
            printf("rp: 4\n");  //test
            
            return 1;
        }
        
    }
}

void schedulingRR(int pauseCause)
{
    //move first readyProcess to running
    gll_push(runningProcess, gll_first(readyProcess));
    gll_pop(readyProcess);
    
    
    
    if(gll_first(runningProcess) != NULL)
    {
        current_time = current_time + contextSwitchTime;
        OSTime += contextSwitchTime;
        numberContextSwitch++;
        struct PCB* temp = gll_first(runningProcess);
        gll_pushBack(resultStats.executionOrder, temp->name);
        
        int i;
        //printf("a");
        for(i=0; i<16; i++){
          
          unsigned int* add = (unsigned int*)gll_pop(TLBList);
          free(add);
          add = calloc(1, sizeof(unsigned int));
          gll_pushBack(TLBList, add);
        }
        
    }
}

/*runs a process. returns 0 if page fault, 1 if quanta finishes, -1 if traceFile ends, 2 if no running process, 4 if disk Interrupt*/
int processSimulator()
{   
    uint64_t stopTime = nextQuanta;
    int stopCondition = 1;
    if(gll_first(runningProcess)!=NULL)
    {
        //TODO
        if((disk_interrupt_clock + sysParam->Swap_latency) > stopTime) 
        {
            //TODO: stopTime = occurance of the first disk interrupt
                stopCondition = 4;
                current_time += sysParam->Swap_interrupt_handling_time; 
                nextQuanta += sysParam->Swap_interrupt_handling_time; 
                OSTime += sysParam->Swap_interrupt_handling_time; 
        }
        
        while(current_time < stopTime)
        {
            
            int read = readPage(gll_first(runningProcess), stopTime);
            if(debug == 1){
                printf("Read: %d\n", read);
                printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 " %" PRIu64 "\n",current_time, nextQuanta, stopTime);
            }
            if(read == 0)
            {
                return -1;
                break;
            }
            else if(read == -1) //page fault
            {
                if(gll_first(runningProcess) != NULL)
                {
                    gll_pushBack(blockedProcess, gll_first(runningProcess));
                    gll_pop(runningProcess);

                    return 0;
                }
                
            }
        }
        if(debug == 1)
        {
            printf("Stop condition found\n");
            printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 "\n",current_time, nextQuanta);
        }
        return stopCondition;
    }
    if(debug == 1)
    {
        printf("No running process found\n");
    }
    return 2;
}

void cleanUpProcess(struct PCB* p)
{
    //struct PCB* temp = gll_first(runningProcess);
    struct PCB* temp = p;
   //TODO: Adjust the amount of available memory as this process is finishing
    
    struct Stats* s = malloc(sizeof(stats));
    s->processName = temp->name;
    s->hitCount = temp->hitCount;
    s->missCount = temp->missCount;
    s->user_time = temp->user_time;
    s->OS_time = temp->OS_time;

    s->duration = current_time - temp->start_time;
    
    gll_pushBack(resultStats.perProcessStats, s);
    
    gll_destroy(temp->memReq);
    closeTrace(temp->memoryFile);

}

void printPCB(void* v)
{
    struct PCB* p = v;
    if(p!=NULL){
        printf("%s, %" PRIu64 "\n", p->name, p->start_time);
    }
}

void printStats(void* v)
{
    struct Stats* s = v;
    if(s!=NULL){
        double hitRatio = s->hitCount / (1.0* s->hitCount + 1.0 * s->missCount);
        printf("\n\nProcess: %s: \nHit Ratio = %lf \tProcess completion time = %" PRIu64 
                "\tuser time = %" PRIu64 "\tOS time = %" PRIu64 "\n", s->processName, hitRatio, s->duration, s->user_time, s->OS_time) ;
    }
}

void printExecOrder(void* v)
{
    char* c = v;
    if(c!=NULL){
        printf("%s\n", c) ;
    }
}



void diskToMemory()
{   
    struct PCB* temp = gll_first(blockedProcess);
    
    if(temp != NULL){
    printf("func dtm temp\n%p\n",temp);
    //printf("zuichudeqidian0: %p\n",temp->vphead);
    
    
    //printf("func dtm start\n"); //test
    gll_t *VPList;
    // TODO: Move requests from disk to memory
    // TODO: move appropriate blocked process to ready process
    
    /*
    struct ppNode* tempp = gll_pop(PPTList);
    struct PCB* tempPCB = tempp->pAddr;
    struct NextMem* paddr= tempp->addr;
    */
    
    //printf("func dtm paddr\n%p\n",paddr);
    /*if(paddr != NULL){
    
    unsigned int paddress = (unsigned int) strtol(paddr->address, NULL, 16);
    
    unsigned int pl1 = paddress >> (32 - sysParam->N1_in_bits);
    unsigned int pl2 = paddress << (sysParam->N1_in_bits);
    pl2 = pl2 >> (32 - sysParam->N2_in_bits);
    unsigned int pl3 = paddress << (sysParam->N1_in_bits + sysParam->N2_in_bits);
    pl3 = pl3 >> (32 - sysParam->N3_in_bits);
    //gll_set();// set the pg block into 0
    
    }*/
    
    
    struct NextMem* addr = gll_first(temp->memReq);
    
    
    
    if(temp->vphead == NULL){
    VPList = gll_init();
    int i;
    for(i=0; i<power(sysParam->N1_in_bits); i++){
        struct VP *l1vp = malloc(sizeof(struct VP));
        l1vp->v = 0;
        l1vp->next_addr = NULL;
        gll_pushBack(VPList, l1vp);
    }
    
    
    
    temp->vphead = VPList;
    }
    unsigned int address = (unsigned int) strtol(addr->address, NULL, 16);
    unsigned int l1 = address >> (32 - sysParam->N1_in_bits);
    unsigned int l2 = address << (sysParam->N1_in_bits);
    l2 = l2 >> (32 - sysParam->N2_in_bits);
    
    unsigned int l3 = address << (sysParam->N1_in_bits + sysParam->N2_in_bits);
    l3 = l3 >> (32 - sysParam->N3_in_bits);
    printf("func dtm cp1\n"); //test
    // pushBack ppNode into PPTList
    struct ppNode* ppn= gll_pop(PPTList);
    printf("func dtm cp2\n"); //test
    if(ppn->valid == 1){
        printf("func dtm cp3\n"); //test
        unsigned int pl1 = ppn->VPN >> (32 - sysParam->N1_in_bits);
        unsigned int pl2 = ppn->VPN << (sysParam->N1_in_bits);
        pl2 = pl2 >> (32 - sysParam->N2_in_bits);
        unsigned int pl3 = ppn->VPN << (sysParam->N1_in_bits + sysParam->N2_in_bits);
        pl3 = pl3 >> (32 - sysParam->N3_in_bits);
        printf("func dtm cp4\n"); //test
        printf("ppn->vphead: %p\n",ppn->vphead); //test
        struct VP* ivvp = (VP*)gll_get((((VP*)gll_get(((VP*)gll_get(ppn->vphead, pl1))->next_addr, pl2))->next_addr),pl3);
        printf("func dtm cp5\n"); //test
        ivvp ->v = 0;
        gll_set((gll_get(((VP*)gll_get(ppn->vphead, l1))->next_addr, l2)), ivvp, l3);
        printf("func dtm cp6\n"); //test
    }
    printf("func dtm cp7\n"); //test
    ppn->valid = 1;
    ppn->VPN = address;
    ppn->vphead = temp->vphead;
    gll_pushBack(PPTList, ppn);
    
    
    //printf("zuichudeqidian1: %p\n",temp->vphead);
    
    

    
   
    //printf("data ptr: %p\n",(VP*)gll_get((temp->vphead)->next_addr, l1));
    printf("temp->vphead: %p\n",temp->vphead);
    
    
    struct VP* l1vp = (VP*)gll_get((temp->vphead), l1);
    
    printf("l1vp->next_addr: %p\n", l1vp->next_addr); //test
    l1vp->v = 1;
    
    //printf("l1vp %p",(VP*)gll_get((temp->vphead), l1));
    if(l1vp->next_addr == NULL){
        
        VPList = gll_init();
        int i;
        for(i=0; i<power(sysParam->N2_in_bits); i++){
            struct VP *l2vp = malloc(sizeof(struct VP));
            l2vp->v = 0;
            l2vp->next_addr = NULL;
            gll_pushBack(VPList, l2vp);
        }
        
        l1vp->next_addr = VPList;
    }
    
    //vp = gll_get(temp->vphead, l1);
    
    
    struct VP* l2vp = (VP*)gll_get((l1vp->next_addr),l2);
    l2vp->v = 1;
    
    
    if(l2vp->next_addr == NULL){
        VPList = gll_init();
        int i;
        for(i=0; i<power(sysParam->N3_in_bits); i++){
            struct VP *l3vp = malloc(sizeof(struct VP));
            l3vp->v = 0;
            l3vp->next_addr = NULL;
            gll_pushBack(VPList, l3vp);
        }
        l2vp->next_addr = VPList;
        // cannot set
    }
    struct VP* l3vp = (VP*)gll_get((l2vp->next_addr),l3);
    l3vp->v=1;
    l3vp->next_addr = ppn;
    

    /*
    struct VP* vvp = (VP*)gll_get(((VP*)gll_get(((VP*)gll_get(temp->vphead, l1))->next_addr, l2))->next_addr,l3);
    vvp->v = 1;
    vvp->next_addr = ppn;
    gll_set((gll_get(((VP*)gll_get(temp->vphead, l1))->next_addr, l2)), vvp, l3);
    */
    
    //gll_set(gll_get(temp->vphead, l1), VPList, l2);
    //printf("func dtm md2\n"); //test


    /*
    struct VP* vp = malloc(sizeof(struct VP));
    vp->v = 1;
    vp->next_addr = ppn;
    
    gll_set((gll_t*)gll_get((gll_t*)gll_get(temp->vphead, l1), l2), vp, l3);
    */
    
    /*
    struct ppNode* nPP;
    nPP->pAddr = temp;
    nPP->addr = addr;
    gll_pushBack(PPTList, nPP);
    */
    
    // update TLB list .fin
    struct NextMem* TLBaddr = gll_first(temp->memReq);
    unsigned int addr_tag = (unsigned int)strtol(TLBaddr->address, NULL, 16) >> sysParam->P_in_bits;
    unsigned int* tempTLB = gll_pop(TLBList);
    *tempTLB = addr_tag;
    gll_pushBack(TLBList, tempTLB);
    gll_pushBack(readyProcess, temp);
    gll_pop(blockedProcess);
    printf("func dtm end\n"); //test
    if(debug == 1)
    {
        printf("Done diskToMemory\n");
    }
    //printf("func dtm end\n"); //test
    
    }
}


void simulate()
{
    init();
    statsinit();

    //get the first ready process to running state
    struct PCB* temp = gll_first(readyProcess);
    gll_pushBack(runningProcess, temp);
    gll_pop(readyProcess);

    struct PCB* temp2 = gll_first(runningProcess);
    gll_pushBack(resultStats.executionOrder, temp2->name);

    while(1)
    {
        int simPause = processSimulator();
        if(current_time == nextQuanta)
        {
            nextQuanta = current_time + quantum;
        }

        //transfer ready processes from processList to readyProcess list
        struct PCB* temp = gll_first(processList);
        
        while((temp!= NULL) && ( temp->start_time <= current_time))
        {
            temp->memoryFile = openTrace(temp->memoryFilename);
            temp->numOfIns = readNumIns(temp->memoryFile);

            struct NextMem* tempAddr = readNextMem(temp->memoryFile);

	        while(tempAddr!= NULL)
            {
                gll_pushBack(temp->memReq, tempAddr);
                tempAddr = readNextMem(temp->memoryFile);
            }
            gll_pushBack(readyProcess, temp);
            gll_pop(processList);

            temp = gll_first(processList);
        }

        //move elements from disk to memory
        diskToMemory();

        //This memory trace done
        if(simPause == -1)
        {
            //finish up this process
            cleanUpProcess(gll_first(runningProcess));
            gll_pop(runningProcess);
        }

        //move running process to readyProcess list
        int runningProcessNUll = 0;
        if(simPause == 1 || simPause == 4)
        {
            if(gll_first(runningProcess) != NULL)
            {
                gll_pushBack(readyProcess, gll_first(runningProcess));
                gll_pop(runningProcess);
            }
            else{
                runningProcessNUll = 1;
            }
            if(simPause == 1)
            {
                nextQuanta = current_time + quantum;
            }
        }

        schedulingRR(simPause);

        //Nothing in running or ready. need to increase time to next timestamp when a process becomes ready.
        if((gll_first(runningProcess) == NULL) && (gll_first(readyProcess) == NULL))
        {
            if(debug == 1)
            {
                printf("\nNothing in running or ready\n");
            }
            if((gll_first(blockedProcess) == NULL) && (gll_first(processList) == NULL))
            {

                    if(debug == 1)
                    {
                        printf("\nAll done\n");
                    }
                    break;
            }
            struct PCB* tempProcess = gll_first(processList);
            struct PCB* tempBlocked = gll_first(blockedProcess);

            //TODO: Set correct value of timeOfNextPendingDiskInterrupt
            uint64_t timeOfNextPendingDiskInterrupt = 0;

            if(tempBlocked == NULL)
            {
                if(debug == 1)
                {
                    printf("\nGoing to move from proess list to ready\n");
                }
                struct NextMem* tempAddr;
                tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                tempAddr = readNextMem(tempProcess->memoryFile);
                while(tempAddr!= NULL)
                {
                    gll_pushBack(tempProcess->memReq, tempAddr);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                }
                gll_pushBack(readyProcess, tempProcess);
                gll_pop(processList);
                
                while(nextQuanta < tempProcess->start_time)
                {   
                    
                    current_time = nextQuanta;
                    nextQuanta = current_time + quantum;
                }
                OSTime += (tempProcess->start_time-current_time);
                current_time = tempProcess->start_time; 
            }
            else
            {
                if(tempProcess == NULL)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else if(tempProcess->start_time >= timeOfNextPendingDiskInterrupt)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else{
                    struct NextMem* tempAddr;
                    if(debug == 1)
                    {
                        printf("\nGoing to move from proess list to ready\n");
                    }
                    tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                    tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                    while(tempAddr!= NULL)
                    {
                        gll_pushBack(tempProcess->memReq, tempAddr);
                        tempAddr = readNextMem(tempProcess->memoryFile);
                    }
                    gll_pushBack(readyProcess, tempProcess);
                    gll_pop(processList);
                    
                    while(nextQuanta < tempProcess->start_time)
                    {   
                        current_time = nextQuanta;
                        nextQuanta = current_time + quantum;
                    }
                    OSTime += (tempProcess->start_time-current_time);
                    current_time = tempProcess->start_time; 
                }
            }   
        }
    }
}

int main(int argc, char** argv)
{   printf("func main start\n");//test
    if(argc == 1)
    {
        printf("No file input\n");
        exit(1);
    }
    traceFileName = argv[1];
    outputFileName = argv[2];

    simulate();
    printf("func sim finished\n"); //test
    finishAll();
    printf("func fa finished\n"); //test
    statsUpdate();printf("func su finished\n"); //test

    if(writeToFile(outputFileName, resultStats) == 0)
    {
        printf("Could not write output to file\n");
    }
    printf("User time = %" PRIu64 "\nOS time = %" PRIu64 "\n", resultStats.userModeTime, resultStats.OSModetime);
    printf("Context switched = %d\n", resultStats.numberOfContextSwitch);
    printf("Start time = 0\nEnd time =%llu", current_time);
    gll_each(resultStats.perProcessStats, &printStats);

    // printf("\nExec Order:\n");
    // gll_each(resultStats.executionOrder, &printExecOrder);
    printf("\n");
}
