#define DEBUG2 1
#pragma once

typedef struct process process;
typedef struct process *pProcess;
typedef struct semaphore semaphore;
typedef struct list *list;    // Linked list of processes (can be children list or waiting list)

struct process {                        // A process
   pProcess pNext;            // Next pointer
   pProcess pPrev;            // Prev pointer

   int      pid;
   int      parentPid;
   int      (*entryPoint)(char *); // entry point
   char     *name;            // name
   int      priority;         // Process's priority
   int      status;           // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      privateMbox;      // Private mailbox ID
   int      startupMbox;      // Startup mailbox ID
   int      cpu_time;         // Time on CPU
   int      tsStart;          // Time the process started executing
   int      tsEnd;            // Time when process stopped executing
   int      termCode;         // Termination code
   int      child_waiting;    // 0 for neither, 1 for child, 2 for waiting

   list children;         // Linked list of children processes
};

struct semaphore {   // A semaphore
   int      sid;     // Semaphore ID
   int      value;   // Semaphore value
   int      status;  // Semaphore status
   int      mbox;    // Semaphore mailbox ID (CURRENTLY NOT USED AT ALL)
   int      mutex;   // Mutex for manipulating semaphore's value

   list  waiting; // waiting list of processes waiting on this semaphore
};

struct list {       // List of children processes
   pProcess pHead;      // Pointer to head process
   pProcess pTail;      // Pointer to tail process
   int      count;      // Count of children processes
};

// constants
#define SYS_SEMCREATE 20                // choosing 20 at random, no purpose
#define ITEM_IN_USE 1
#define SEM_UNUSED 5
#define SEM_USED 6
#define SEM_FREE 7            // Indicates a sempaphore is free
#define SEM_BLOCKED 8         // Indicates a semaphore is blocked

#define STATUS_RUNNING 9
#define STATUS_CPUCALC 10
#define STATUS_TERMINATED 11
