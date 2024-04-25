#define DEBUG4 1
#define debugflag4 2 // not sure what this is
#pragma once

typedef struct disk_request *disk_request;
typedef struct process process;
typedef struct process *pProcess;
typedef struct list *list;

struct disk_request {
   disk_request pNext;
   disk_request pPrev;

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
   int      priority;               // Process's priority
   int      status;                 // Int to hold process status (Used, Unused, Ready, Not Ready, etc.)
   int      privateMbox;            // Private mailbox ID
   int      isZapped;               // Flag to measure if process has been zapped or not

   /* Sleep/Clock items */
   double   sleepStartTime;         // Time the process went to sleep (for calculation)
   double   sleepEndTime;           // Number of seconds to sleep
   int      sleepSem;               // Semaphore used for sleeping synchronization

   /* Disk items */
   struct disk_request diskRequest;  // Process's disk request

};

struct list {        // List of processes
   pProcess pHead;   // Pointer to head process
   pProcess pTail;   // Pointer to tail process
   int      count;   // Count of processes
};

/* Constants */
#define STATUS_RUNNING 11
#define STATUS_SLEEPING 12