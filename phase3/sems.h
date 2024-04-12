#define DEBUG2 1
#pragma once

typedef struct process process;
typedef struct process *pProcess;
typedef struct semaphore semaphore;
typedef struct children *children;  // Linked list of children processes for any process

struct process {                        // A process
   pProcess pNext;            // Next pointer
   pProcess pPrev;            // Prev pointer

   int      pid;
   int      parentPid;
   int      (*entryPoint)(char *); // entry point
   char     name[MAXNAME];    // name
   int      status;           // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      privateMbox;      // Private mailbox ID
   int      startupMbox;      // Startup mailbox ID

   children children;         // Linked list of children processes
};

struct semaphore {   // A semaphore
   int      sid;     // Semaphore ID
   int      value;   // Semaphore value
   int      status;  // Semaphore status
   int      mbox;    // Semaphore mailbox ID
   // waiting list of processes waiting on this semaphore?
};

struct children {       // List of children processes
   pProcess pHead;      // Pointer to head process
   pProcess pTail;      // Pointer to tail process
   int      count;      // Count of waiting processes
};


// constants
#define SYS_SEMCREATE 11                // choosing 11 at random, no purpose
#define ITEM_IN_USE 1
#define SEM_FREE 5            // Indicates a sempaphore is free
#define SEM_USED 6
#define SEM_BLOCKED 7         // Indicates a semaphore is blocked