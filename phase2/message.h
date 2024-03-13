#define DEBUG2 1

typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;        // Not sure where this comes in yet
typedef struct mail_slot *slot_ptr;             // Mail slot
typedef struct slot_list *slot_list;            // Linked list of slots for a mailbox
typedef struct waiting_proc *waiting_proc_ptr;  // Waiting process
typedef struct waiting_list *waiting_list;      // Linked list of waiting processes for a mailbox


struct mailbox {                       // A mailbox
   int      mbox_id;
   /* other items as needed... */
   int      status;                    // Int to hold mailbox status (Used, Unused, etc.)
   int      available_messages;        // Int to hold the count of available messages in this mbox (for recieve)
   slot_list slot_list;                // Linked list of slots (ptr)
   waiting_list waiting_list;          // List of processes that are waiting to receive (ptr)
};

/* SLOTS */
struct mail_slot {                     // A slot in the mailbox (doubly linked)
   int       mbox_id;
   int       slot_id;               
   int       status;
   /* other items as needed... */
   char message[MAX_MESSAGE];          // A string to hold the message 
   slot_ptr prev_slot;                 // Pointer to the previous slot in the mailbox
   slot_ptr next_slot;                 // Pointer to the next slot in the mailbox
};

struct slot_list {                     // Linked list of slots for a mailbox
   slot_ptr head_slot;
   slot_ptr tail_slot;
   int mbox_id;
   int count;
};

/* WAITING */
struct waiting_proc              // Waiting process
{
   int pid;                      // pid of waiting process
   int mbox_id;                  // Mailbox ID of the mailbox this process is waiting on
   waiting_proc_ptr pNext;       // Pointer to next process in waiting list
   waiting_proc_ptr pPrev;       // Pointer to prev process in waiting list
   // Add Process info here
};

struct waiting_list {            // List of waiting processes
   waiting_proc_ptr pHead;       // Pointer to head process
   waiting_proc_ptr pTail;       // Pointer to tail process
   int mbox_id;                  // ID of mailbox this list is assigned to
   int count;                    // Count of waiting processes
};

struct psr_bits {
    unsigned int cur_mode:1;
    unsigned int cur_int_enable:1;
    unsigned int prev_mode:1;
    unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

// Process table? (use getnextpid() % MAXPROC to get new pids for new proc table)

/* CONSTANTS */
#define STATUS_UNUSED -1
#define STATUS_EMPTY 0
#define STATUS_USED 1