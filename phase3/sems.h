#define DEBUG2 1
#pragma once

typedef struct process process;
typedef struct process* pProcess;
typedef struct semaphore semaphore;

struct process {                        // A process
   pProcess pNext;            // Next pointer
   pProcess pPrev;            // Prev pointer

   int      pid;
   int      parentPid;
   int      (*entryPoint)(char *); // entry point
   char     name[MAXNAME];       // name
   int      status;           // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      privateMbox;
   int      start_mbox;        // - do we need to create a mailbox table too? 
};

struct semaphore {   // A semaphore
   int      sid;     // Semaphore ID
   int      count;   // Semaphore count
   // waiting list of processes waiting on this semaphore?
};


// constants
#define SYS_SEMCREATE 11                // choosing 11 at random, no purpose
#define ITEM_IN_USE 1