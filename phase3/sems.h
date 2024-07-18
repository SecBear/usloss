#define DEBUG2 1
#pragma once

typedef struct process process;
typedef struct process *pProcess;
typedef struct semaphore semaphore;
typedef struct list *list;

struct process {              // A process
   pProcess pNext;            // Next pointer
   pProcess pPrev;            // Prev pointer

   int      pid;
   int      parentPid;
   int      (*entryPoint)(char *);  // Entry point
   char     *name;                  // Name
   int      priority;               // Process's priority
   int      status;                 // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      privateMbox;            // Private mailbox ID
   int      startupMbox;            // Startup mailbox ID
   int      termCode;               // Termination code
   int      child_waiting;          // Flag to indicate child or waiting process: 0 for neither, 1 for child, 2 for waiting

   list children;                   // Linked list of children processes
};

struct semaphore {   // A semaphore
   int      sid;     // Semaphore ID
   int      value;   // Semaphore value
   int      status;  // Semaphore status
   int      mutex;   // Mutex for manipulating semaphore's value

   list  waiting;    // waiting list of processes waiting on this semaphore
};

struct list {        // List of processes
   pProcess pHead;   // Pointer to head process
   pProcess pTail;   // Pointer to tail process
   int      count;   // Count of processes
};

// Constants
#define SEM_UNUSED 5
#define SEM_USED 6
#define SEM_FREE 7            
#define STATUS_RUNNING 9
#define STATUS_TERMINATED 11
