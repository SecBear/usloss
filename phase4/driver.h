#define DEBUG4 1
#define debugflag4 2 // not sure what this is
#pragma once

typedef struct disk_request *pdisk_request;
typedef struct disk_request disk_request;
typedef struct process process;
typedef struct process *pProcess;
typedef struct list *list;
typedef struct request_list *request_list;

struct disk_request {
   pdisk_request pNext;
   pdisk_request pPrev;

   /* Used for disk requests */
   int   operation;    /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int   track_start;
   int   sector_start;
   int   num_sectors;
   void *disk_buf;
   int   unit;          // Disk unit to read

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
   pdisk_request diskRequest;  // Process's disk request

};

struct list {        // List of processes
   pProcess pHead;   // Pointer to head process
   pProcess pTail;   // Pointer to tail process
   int      count;   // Count of processes
   int      type;    // 0 = list, 1 = request list
};

struct request_list {
   pdisk_request pHead;
   pdisk_request pTail;
   int          count;
   int           type;
};

/* Constants */
#define STATUS_RUNNING 11
#define STATUS_SLEEPING 12
#define TYPE_REQUEST_LIST 13
#define TYPE_LIST 14