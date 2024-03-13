#define DEBUG2 1

typedef struct mailbox mail_box;
typedef struct mail_slot mail_slot;
typedef struct mbox_proc mbox_proc;
typedef struct mbox_proc *mbox_proc_ptr;        // Mailbox process?
typedef struct mail_slot *slot_ptr;             // Mail slot
typedef struct slot_list *slot_list;            // Linked list of slots for a mailbox
typedef struct waiting_proc *waiting_proc_ptr;  // Waiting process
typedef struct waiting_list *waiting_list;      // Linked list of waiting processes for a mailbox


struct mailbox {                       // A mailbox
   int      mbox_id;
   /* other items as needed... */
   int      status;                    // Int to hold mailbox status (Used, Unused, etc.)
   int      available_messages;        // Int to hold the count of available messages in this mbox (for recieve)
   int      zero_slot;                 // 0 or 1 to indicate whether or not this is a zero-slot mailbox (initialized to -1)
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

/* PROCESS */
struct mbox_proc
{
   int pid;
   int status;
   char message[MAX_MESSAGE];
};

/* WAITING */
struct waiting_proc              // Waiting process
{
   int mbox_id;                  // Mailbox ID of the mailbox this process is waiting on
   waiting_proc_ptr pNext;       // Pointer to next process in waiting list
   waiting_proc_ptr pPrev;       // Pointer to prev process in waiting list
   // Add Process info here
   mbox_proc_ptr process;        // The process
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
#define STATUS_WAIT_SEND 11       // Waiting to send to a receiver
#define STATUS_WAIT_RECEIVE 12    // Waiting to receive from a sender