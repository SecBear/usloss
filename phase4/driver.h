#define DEBUG2 1
#pragma once

typedef struct driver_proc * driver_proc_ptr;
typedef struct process process;
typedef struct process *pProcess;
typedef struct list *list;

struct driver_proc {
   driver_proc_ptr next_ptr;

   int   wake_time;    /* for sleep syscall */
   int   been_zapped;


   /* Used for disk requests */
   int   operation;    /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int   track_start;
   int   sector_start;
   int   num_sectors;
   void *disk_buf;

   //more fields to add

};

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

struct list {        // List of processes
   pProcess pHead;   // Pointer to head process
   pProcess pTail;   // Pointer to tail process
   int      count;   // Count of processes
};