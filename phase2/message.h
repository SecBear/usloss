#define DEBUG2 1

typedef struct mail_slot *slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;  // Not sure where this comes in yet
typedef struct WAITINGPROC *waiting_list; // List of waiting processes
typedef struct slot_list *slot_list;      // Linked list of slots for a mailbox

struct mailbox {                       // A mailbox
   int      mbox_id;
   /* other items as needed... */
   int      status;                    // Int to hold mailbox status (Used, Unused, etc.)
   int      available_messages;        // Int to hold the count of available messages in this mbox (for recieve)
   slot_list slot_list;                // Linked list of slots (ptr)
   waiting_list waiting_list;          // List of processes that are waiting to receive (ptr)
};

struct mail_slot {                     // A slot in the mailbox (doubly linked)
   int       mbox_id;
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

// Waiting list
struct WAITINGPROC
{
   int pid;                      // pid of waiting process
   // Add Process info here
   struct next_proc *next_ptr;   // pointer to the next process that's waiting
   int count;                    // count of processes waiting
};

// Process table? (use getnextpid() % MAXPROC to get new pids for new proc table)

/* CONSTANTS */
#define STATUS_EMPTY 0