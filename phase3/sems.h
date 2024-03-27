#define DEBUG2 1
#pragma once

typedef struct process process;
typedef struct semaphore semaphore;

struct process {                        // A process
   int      pid;
   /* other items as needed... */
   int      status;        // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      start_mbox      // - do we need to create a mailbox table too? 
};

struct semaphore {   // A semaphore
   int      sid;     // Semaphore ID
   int      count;   // Semaphore count
   // waiting list of processes waiting on this semaphore?
};


// constants
#define SYS_SEMCREATE 11                // choosing 11 at random, no purpose
#define ITEM_IN_USE 1