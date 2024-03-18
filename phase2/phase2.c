/* ------------------------------------------------------------------------
   phase2.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   Bryce Thorpe & Vivek Madala

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1(char *);
extern int start2(char *);
void check_kernel_mode(char string[]);
void disableInterrupts();
static void enableInterrupts();
int MboxCreate(int slots, int slot_size);
int MboxSend(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size);
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size);
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size);
int AddToWaitList(int mbox_id, int status, void *msg_ptr, int msg_size);
int GetNextMboxID();
slot_ptr GetNextReadySlot(int mbox_id);
void SlotListInit(mail_box *mbox, int slots, int slot_size);
int GetNextSlotID();
void clock_handler2();
void disk_handler();
void term_handler();
void sys_handler();
static void nullsys();
static sysargs *args = NULL;
int SysVec();

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// Mailboxes & slots
mail_box MailBoxTable[MAXMBOX];   // Array of 2000 mailboxes
mail_slot MailSlotTable[MAXSLOTS]; // Array of 2500 Mail slot pointers (NOTE: slot array/table is not the slot list)
mbox_proc ProcTable[MAXPROC];     // Array of processes

/* array of mail box processes (proc table) */
// static struct mbox_proc MboxProcs[MAXSLOTS]; // NOTE: use `i = getpid()%MAXPROC` to get the next pid

int slot_count = 0; // Integer to keep track of total number of slots

unsigned int next_mbox_id = 0; // The next mbox_id to be assigned
unsigned int next_slot_id = 0; // The next slot_id to be assigned
int numMbox = 0;               // Number of currently active mailboxes
int numSlot = 0;               // Number of currently active slots
int numWaitingProc = 0;        // Number of waiting processes

int clock_count = 0;    // Count to keep track of clock_handler calls
int waiting_for_io = 0; // Count to keep track of processes waiting for I/O

// Interrupt Mailboxes
int clock_mbox;
int disk_mbox[2];
int term_mbox[4];
// int clock_mbox[];

void (*sys_vec[MAXSYSCALLS])(sysargs *args); // for system call handler

/* -------------------------- Functions -----------------------------------
  Below I have code provided to you that calls

  check_kernel_mode
  enableInterrupts
  disableInterupts

  These functions need to be redefined in this phase 2,because
  their phase 1 definitions are static
  and are not supposed to be used outside of phase 1.  */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
   int kid_pid, status;

   if (DEBUG2 && debugflag2)
   {
      console("start1(): at beginning\n");
   }

   check_kernel_mode("start1");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures. */

   // Mail box table
   for (int i = 0; i < MAXMBOX; i++)
   {
      // Status, ID and available messages
      MailBoxTable[i].mbox_id = STATUS_UNUSED;
      MailBoxTable[i].status = STATUS_EMPTY;
      MailBoxTable[i].available_messages = STATUS_EMPTY;
      MailBoxTable[i].zero_slot = STATUS_UNUSED;
   }

   // Slot table
   for (int i = 0; i < MAXSLOTS; i++)
   {
      // Status, mailbox ID, slot ID, status, next and prev slot
      MailSlotTable[i].status = STATUS_UNUSED;
      MailSlotTable[i].mbox_id = STATUS_UNUSED;
      MailSlotTable[i].slot_id = i;
      MailSlotTable[i].message[MAX_MESSAGE] = '-1';
      MailSlotTable[i].next_slot = NULL;
      MailSlotTable[i].prev_slot = NULL;
   }

   // Proc table
   for (int i = 0; i < MAXPROC; i++)
   {
      // PID, status, Message
      ProcTable[i].status = STATUS_EMPTY;
      ProcTable[i].pid = STATUS_UNUSED;
      ProcTable[i].message[MAX_MESSAGE] = '-1';
   }

   /* Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

   int_vec[CLOCK_DEV] = clock_handler2;    // clock handler
   int_vec[DISK_DEV] = disk_handler;       // disk handler
   int_vec[TERM_DEV] = term_handler;       // terminal handler
   int_vec[SYSCALL_INT] = sys_handler; // System call handler
  

   clock_mbox = MboxCreate(0, 0); // clock mailbox

   for (int i = 0; i < 2; i++) // disk mailboxes
   {
      disk_mbox[i] = MboxCreate(0, 0); // I/O mailboxes for disks
   }

   for (int i = 0; i < 4; i++) // Term mailboxes
   {
      term_mbox[i] = MboxCreate(0, 0); // I/O mailboxes for terminals
   }

   for (int i = 0; i < MAXSYSCALLS; i++)
   {
      sys_vec[i] = nullsys; // initialize every system call handler as nullsys
   }

   enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");
   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   if (join(&status) != kid_pid)
   {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */

void check_kernel_mode(char string[])
{
   int currentPsr = psr_get();

   // if the kernel mode bit is not set, then halt
   // meaning if not in kernel mode, halt(1)
   if ((currentPsr & PSR_CURRENT_MODE) == 0)
   {
      // not in kernel mode
      console("%s, Kernel mode expected, but function called in user mode.\n", string);
      halt(1);
   }
}

void disableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
   check_kernel_mode("disableInterrupts");

   /* We ARE in kernel mode */
   psr_set(psr_get() & ~PSR_CURRENT_INT);

} /* disableInterrupts */

static void enableInterrupts()
{
   int currentPsr = psr_get();                         // Get current psr
   int interruptEnable = currentPsr | PSR_CURRENT_INT; // Set the interrupt enable bit to ON (0x2)
   psr_set(interruptEnable);                           // Set psr to new psr
}

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
   int mbox_id;
   // Check kernel mode?

   // Check for simple errors
   if (slots < 0 || slot_size < 0 || slots + slot_count > MAXSLOTS || slot_size > MAX_MESSAGE) // If we're trying to create too many slots
   {
      return -1;
   }
  
   // Get the next mailbox ID
   mbox_id = GetNextMboxID();

   if (slots == 0) // If this is a zero slot mailbox,
   {
      MailBoxTable[mbox_id].zero_slot = 1; // Zet zero_slot flag to 1
   }
   else // If not,
   {
      MailBoxTable[mbox_id].zero_slot = 0;                    // Set zero_slot flag to 0
      SlotListInit(&MailBoxTable[mbox_id], slots, slot_size); // Initialize slots & slot list
   }

   WaitingListInit(&MailBoxTable[mbox_id]); // Initialize waiting list

   MailBoxTable[mbox_id].mbox_id = mbox_id;    // Update mailbox ID
   MailBoxTable[mbox_id].status = STATUS_USED; // Update mailbox status
   numMbox++;                                  // Increment number of mailboxes

   return mbox_id;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) // atomic (no need for mutex or semaphore, etc. note: interrupts are disabled)
{
   int needToUnblock = 0;
   int pid = -1;

   check_kernel_mode("MboxSend\n");

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || MailBoxTable[mbox_id].status < 1)
   {
      // Error message here
      return -1;
   }
   if (MailBoxTable[mbox_id].zero_slot == 0)
   {
      if (msg_size > MailBoxTable[mbox_id].slot_list->slot_size)
      {
         return -1;
      }
   }

   // Get the mailbox from the mail box table
   mail_box *mbox = &MailBoxTable[mbox_id];

   // Is the message NULL? if so, set proper flag

   // Is anyone waiting? (check waiting list and wake up the first process to start waiting)
   // if so, we need to copy that data into the mailbox and unblock the process that's waiting
   if (mbox->waiting_list->count > 0) // Is anyone waiting on this mailbox?
   {
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_RECEIVE) // If this process is waiting to recieve
         {
            // If this is a zero slot mailbox, send process directly (trying to just add to wait)
            if (mbox->zero_slot)
            {
               // If there is a message to send
               // Get message from msg ptr
               if (*(char*)msg_ptr != '-1')// if we're dealing with a message,
               {
                  memcpy(current->process->message, msg_ptr, msg_size);
               }
               pid = current->process->pid;
               // set the flag to unblock proc once message sent
               current->process->delivered = 1;
               unblock_proc(pid);
               return 0; 
            }
            // else, send the message to next available mailbox and unblock proc
            pid = current->process->pid;
            // Remove process from waiting list
            popWaitList(mbox_id);
            // set the flag to unblock proc once message sent
            needToUnblock = 1;
            break;
         }
         current = current->pNext; // Check the next wating process
      }
   } // Else, nobody is waiting

   // If we're zero slot, we won't search for a slot
   if (mbox->zero_slot == 0)
   {
      // If slot is available in this mailbox, allocate a slot from your mail slot table and send it
      if (mbox->slot_list->count < mbox->slot_list->slot_count) // slot is available for allocation
      {  
         // Get next slot id and assign this slot to the table
         int slot_id = GetNextSlotID();
         slot_ptr new_slot = &MailSlotTable[slot_id];
         // Initialize other fields
         new_slot->mbox_id = mbox->mbox_id;
         new_slot->status = STATUS_USED;

         // Copy the message into the slot
         memcpy(new_slot->message, msg_ptr, msg_size);

         // Link the new slot to the mailbox's linked list of slots
         new_slot->next_slot = NULL;                       // New slot's pNext is NULL
         new_slot->prev_slot = mbox->slot_list->tail_slot; // Old tail is new slot's pPrev
         if (mbox->slot_list->head_slot == NULL)
         {                                         // If the list's head is NULL,
            mbox->slot_list->head_slot = new_slot; // New slot is the new head
         }
         else
         {                                                    // Otherwise,
            mbox->slot_list->tail_slot->next_slot = new_slot; // New slot is the tail's pNext
         }
         mbox->slot_list->tail_slot = new_slot; // New slot is now the tail
         mbox->slot_list->count++;              // Increment the slot list count
         numSlot++;                             // increment global list of slots

         // Increment the mailbox's available_messages
         mbox->available_messages++;

         // Deallocation occurs on receive

         if (needToUnblock)
         {
            unblock_proc(pid);
         }

         return 0;
      }
   }

   // check if we need to unblock a process
   if (needToUnblock)
   {
      unblock_proc(pid);
   }

   // No available slot,
   // block the sender, add sender to waiting list
   AddToWaitList(mbox_id, STATUS_WAIT_SEND, msg_ptr, msg_size); // Add this process to waiting list, waiting to send
   block_me(STATUS_WAIT_SEND);                                  // Sender is waiting to send
   // Check if mailbox has been released while blocked
   if (mbox->status == STATUS_RELEASED)
   {
      int pid = getpid();
      waiting_proc_ptr current_proc = mbox->waiting_list->pHead;  // Get current proc
      while (current_proc != NULL)
      {
         if (current_proc->process->pid == pid) // If we've got our current process
         {
            current_proc->process->status = STATUS_EMPTY;   // Set this process's status to empty as to not get zapped
         }
         current_proc = current_proc->pNext; // Get next process
      }
      return -3;  // Mailbox released while waiting
   }

   return 0;

} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Attempts to send a message to the specified mailbox without blocking
             the sender. If the mailbox is full or a zero-slot mailbox without
             a waiting receiver, the message is not sent, and the function
             returns immediately with a status indicating the condition.
   Parameters - int mbox_id: the ID of the mailbox to send the message to.
                void *msg_ptr: pointer to the message data to send.
                int msg_size: size of the message data in bytes.
   Returns - 0 if the message is successfully sent.
             -1 if there's an error with the input parameters or mailbox state.
             -2 if the mailbox is full and the message cannot be sent without blocking.
   Side Effects - If there's a process waiting to receive on the mailbox and the
                  conditions allow, the message is delivered directly to this
                  receiver, potentially unblocking it.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) // non-blocking send
{
   int needToUnblock = 0;
   int pid = -1;

   check_kernel_mode("MboxSend\n");

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || MailBoxTable[mbox_id].status < 0)
   {
      // Error message here
      return -1;
   }

   // Get mailbox
   mail_box *mbox = &MailBoxTable[mbox_id];

   // Is anyone waiting? (check waiting list and wake up the first process to start waiting)
   // if so, we need to copy that data into the mailbox and unblock the process that's waiting
   if (mbox->waiting_list->count > 0) // Is anyone waiting on this mailbox?
   {
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_RECEIVE) // If this process is waiting to recieve
         {
            // If this is a zero slot mailbox, send process directly (trying to just add to wait)
            if (mbox->zero_slot)
            {
               // If there is a message to send
               // Get message from msg ptr
               if (*(char*)msg_ptr != '-1')// if we're dealing with a message,
               {
                  memcpy(current->process->message, msg_ptr, msg_size);
               }
               pid = current->process->pid;
               // set the flag to unblock proc once message sent
               current->process->delivered = 1;
               unblock_proc(pid);
               return 1; 
            }
            // Do something
            // else, send the message to next available mailbox and unblock proc
            pid = current->process->pid;
            // Remove process from waiting list
            popWaitList(mbox_id);
            // set the flag to unblock proc once message sent
            needToUnblock = 1;
            break;
         }
         current = current->pNext; // Check the next wating process
      }
   } // Else, nobody is waiting

  
   // If we're zero slot, we won't search for a slot
   if (mbox->zero_slot == 0)
   {
      // If we're going to exceed total system number of slots, return -2
      if (numSlot >= MAXSLOTS)
      {
         return -2;
      }

      // If slot is available in this mailbox, allocate a slot from your mail slot table and send it
      if (mbox->slot_list->count < mbox->slot_list->slot_count) // slot is available for allocation
      {  
         // Get next slot id and assign this slot to the table
         int slot_id = GetNextSlotID();
         slot_ptr new_slot = &MailSlotTable[slot_id];
         // Initialize other fields
         new_slot->mbox_id = mbox->mbox_id;
         new_slot->status = STATUS_USED;

         // Copy the message into the slot
         memcpy(new_slot->message, msg_ptr, msg_size);

         // Link the new slot to the mailbox's linked list of slots
         new_slot->next_slot = NULL;                       // New slot's pNext is NULL
         new_slot->prev_slot = mbox->slot_list->tail_slot; // Old tail is new slot's pPrev
         if (mbox->slot_list->head_slot == NULL)
         {                                         // If the list's head is NULL,
            mbox->slot_list->head_slot = new_slot; // New slot is the new head
         }
         else
         {                                                    // Otherwise,
            mbox->slot_list->tail_slot->next_slot = new_slot; // New slot is the tail's pNext
         }
         mbox->slot_list->tail_slot = new_slot; // New slot is now the tail
         mbox->slot_list->count++;              // Increment the slot list count
         numSlot++;                             // increment global list of slots

         // Increment the mailbox's available_messages
         mbox->available_messages++;

         // We deallocate on receive

         if (needToUnblock)
         {
            unblock_proc(pid);
         }

         return 0;
      }
   }

   // check if we need to unblock a process - Add logic for when we're being called from check_io / clock handler
   if (needToUnblock)
   {
      unblock_proc(pid);
   }

   // No available slot
   return -2;
} 

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
  ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size) // atomic (no need for mutex or semaphore, etc. note: interrupts are disabled)
{
   int result = -1;
   int zero_slot = 0;

   check_kernel_mode("MboxReceive\n");

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || MailBoxTable[mbox_id].status < 0)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox
   mail_box *mbox = &MailBoxTable[mbox_id];

   // Check if there are available messages in the mailbox slots
   if (mbox->available_messages > 0)
   {
      slot_ptr slot = GetNextReadySlot(mbox_id);// Get the next ready slot
      if (slot != NULL)                         // If we got one,
      {
         char *message = slot->message;         // Grab the slot's message
         if (message[0] == '\0')
         {
            result = 0;                         // Message is empty
         }
         else
         {
            result = strlen(message) + 1;       // Count message and null-terminator (for output)
         }

         if (result > msg_size)
         {
            return -1;                          // Message is bigger than the buffer size
         }

         memcpy (msg_ptr, message, msg_size);   // Copy message into buffer
         CleanSlot(slot, mbox);                 // Clean out the slot
         return result;
      }
   }

   // Check if there are any processes waiting to send
   if (mbox->waiting_list->count > 0)
   {
      // Find the process already waiting, see if they're waiting to send
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_SEND) // If this process is waiting to send
         {
            popWaitList(mbox_id);       // remove process from waiting list

            // Get the message from the sender directly
            memcpy(msg_ptr, current->process->message, msg_size); // Copy the message including null terminator into receiver's buffer (is this the right buffer?)
            current->process->delivered = 1;                      // Set the process's message delivered flag
            result = strlen(current->process->message) + 1;       // Store the result (the length of message)

            // Clean the buffer
            memset(current->process->message, 0, MAX_MESSAGE); // Zero out the message buffer

            // unblock the waiting process
            unblock_proc(current->process->pid); // Unblock already waiting process

            // Enable/Disable interrupts?

            // Return result
            return result;
         }
         else
         {
            // The process is waiting to receive, so just add this process
            break;
         }
      }
      current = current->pNext; // Check the next wating process
   }
   // This process is the one to receive, 

   // if we're a zero slot, get message from wait list
   if (mbox->zero_slot)
   {
      // find the message
      waiting_proc_ptr current_proc = mbox->waiting_list->pHead;
      while (current_proc != NULL)
      {
         if (current_proc->process->delivered == 1)   // Delivered flag is set
         {
            popWaitList(mbox_id);   // remove process from waiting list
            memcpy(msg_ptr, current_proc->process->message, msg_size); // copy the message
            result = strlen(current_proc->process->message) + 1;       // Store the result (the length of message)
            return result;
         }
         current_proc = current_proc->pNext; // keep looking
      }
      // if no message available, block
      AddToWaitList(mbox_id, STATUS_WAIT_RECEIVE, NULL, -1);
      waiting_for_io++;                // Increment the number of processes waiting for io
      block_me(STATUS_WAIT_RECEIVE);   // Block until a message comes through
      waiting_for_io--;                // Decrement waiting for io

      // Check if mailbox is released
      if (mbox->status == STATUS_RELEASED)
      {
         int pid = getpid();
         waiting_proc_ptr current_proc = mbox->waiting_list->pHead;  // Get current proc
         while (current_proc != NULL)
         {
            if (current_proc->process->pid == pid) // If we've got our current process
            {
               current_proc->process->status = STATUS_EMPTY;   // Set this process's status to empty as to not get zapped
            }
            current_proc = current_proc->pNext; // Get next process
         }
         return -3;
      }

      // messsage should be delivered now
      current_proc = mbox->waiting_list->pHead; 
      while (current_proc != NULL)
      {
         if (current_proc->process->delivered == 1)
         {
            popWaitList(mbox_id);   // remove process from waiting list
            memcpy(msg_ptr, current_proc->process->message, msg_size); // copy the message
            result = strlen(current_proc->process->message) + 1;           // Store the result (the length of message)
            return result;
         }
         current_proc = current_proc->pNext; // keep looking
      }
      popWaitList(mbox_id);

      // if no message available still, we're synchronizing 
      return 0;
   } 

   // block until message is here (using semaphores)
   if (mbox->available_messages <= 0) // If no messages in this mailbox,
   {
      AddToWaitList(mbox_id, STATUS_WAIT_RECEIVE, NULL, -1); // Add to Waiting list of processes to recieve a message?
      block_me(STATUS_WAIT_RECEIVE);                         // Block with status waiting to receive
   }

   // Check if mailbox is released
   if (mbox->status == STATUS_RELEASED)
   {
      int pid = getpid();
      waiting_proc_ptr current_proc = mbox->waiting_list->pHead;  // Get current proc
      while (current_proc != NULL)
      {
         if (current_proc->process->pid == pid) // If we've got our current process
         {
            current_proc->process->status = STATUS_EMPTY;   // Set this process's status to empty as to not get zapped
         }
         current_proc = current_proc->pNext; // Get next process
      }

      return -3;
   }

   // Message is here
   // Grab the next available slot, get it's message and clean the mailbox slot
   slot_ptr slot = GetNextReadySlot(mbox_id); // Get the next slot with a message
   char *message = slot->message;            // Pull its message
   if (message[0] == '\0')
   {
      result = 0;
   }
   else
   {
      result = strlen(message) + 1; // Store the result (the length of message)
   }

   if (result > msg_size)  // Check if the message is bigger than the buffer
   {
      return -1;
   }
   // Put the message from the mailbox slot into the receiver's buffer
   memcpy(msg_ptr, message, msg_size); // Copy the message including null terminator

   // Clean / Deallocate the slot
   CleanSlot(slot, mbox);

   return result; // success
} /* MboxReceive */

int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size) // non-blocking receive
{
   int result = -1;
   int zero_slot = 0;

   // First, check for basic errors
   if (msg_size > MAX_MESSAGE || mbox_id < 0 || mbox_id >= MAXMBOX || msg_ptr == NULL || MailBoxTable[mbox_id].status < 0)
   {
      // Error message here
      return -1;
   }

   // Get the mailbox
   mail_box *mbox = &MailBoxTable[mbox_id];

   // is somebody waiting to send? (receieve from them directly)
   if (mbox->waiting_list->count > 0)
   {
      // Find the process already waiting, see if they're waiting to send
      waiting_proc_ptr current = mbox->waiting_list->pHead; // Start with the head of the waiting list
      while (current != NULL)                               // Loop through Waiting list
      {
         if (current->process->status == STATUS_WAIT_SEND) // If this process is waiting to send
         {
            popWaitList(mbox_id);       // remove process from waiting list

            // Get the message from the sender directly
            memcpy(msg_ptr, current->process->message, msg_size); // Copy the message including null terminator into receiver's buffer (is this the right buffer?)
            current->process->delivered = 1;                      // Set the process's message delivered flag
            result = strlen(current->process->message) + 1;       // Store result

            // Clean the buffer
            memset(current->process->message, 0, MAX_MESSAGE); // Zero out the message buffer

            // unblock the waiting process
            unblock_proc(current->process->pid); // Unblock already waiting process

            // Return result (success)
            return result;
         }
      }
      current = current->pNext; // Check the next wating process
   }

   // This process is the one to receive, 
   // If there are no available messages, return -2
   if (mbox->available_messages <= 0) // If no messages in this mailbox,
   {
      return -2;  // mailbox empty, no message to receive  
   }

   // Message is here
   // Grab the next available slot, get it's message and clean the mailbox slot
   slot_ptr slot = GetNextReadySlot(mbox_id); // Get the next slot with a message
   char *message = slot->message;            // Pull its message
   result = strlen(message) + 1;                  // Store message

   // Put the message from the mailbox slot into the receiver's buffer
   memcpy(msg_ptr, message, msg_size); // Copy the message including null terminator

   // Clean / Deallocate the slot
   CleanSlot(slot, mbox);

   return result; // success
}

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Releases a specified mailbox, making it available for reuse. 
             This function cleans up any allocated resources associated with 
             the mailbox, unblocks any processes waiting on the mailbox, 
             and marks the mailbox as released. If any processes were waiting
             to send or receive messages on this mailbox, they are unblocked
             and may be zapped if they were not in an empty status.
   Parameters - int mbox_id: the ID of the mailbox to release.
   Returns - 0 on successful release of the mailbox.
             -1 if the mailbox is already released, or if an invalid mailbox ID is given.
   Side Effects - Frees up slots and waiting lists associated with the mailbox. 
                  Processes waiting on the mailbox are unblocked and potentially zapped.
                  The mailbox's resources are cleaned up and the mailbox ID is made available for reuse.
   ----------------------------------------------------------------------- */
MboxRelease(int mbox_id)
{  
   mail_box *mbox = &MailBoxTable[mbox_id];

   if (mbox->status != STATUS_USED)
   {
      return -1;
   }

   mbox->status = STATUS_RELEASED; // Set status to released in case of interrutps - may need to set this to STATUS_RELEASING

   /* WAITING LIST */
   // Releaser checks if there are processes blocked on the mailbox
   // How many processes are blocked?
   if (mbox->waiting_list->count > 0)  // There are blocked processes
   {
      // Traverse through and zap unblock_proc(pid) each one
      waiting_proc_ptr current_proc = mbox->waiting_list->pHead;
      while (current_proc != NULL)
      {
         while (mbox->waiting_list->count != 0)
      {
         // Store the next waiting process before freeing current waiting process
         waiting_proc_ptr next_proc = current_proc->pNext;

         unblock_proc(current_proc->process->pid); // Unblock the process (lower priority will return -3 on unblock)
         if (current_proc->process->status != STATUS_EMPTY) // Lower priority processes will have their status set to empty
         {
            zap(current_proc->process->pid);       // Only zaps higher priority processes
         }

         // Clean up the waiting process
         CleanWaitingProc(current_proc, mbox);

         current_proc = next_proc;  // Onto next process
      }
      free(mbox->waiting_list);  // Free the waiting list
      mbox->waiting_list = NULL; // Set the waiting_list pointer to NULL to indicate it's cleaned up

      break;
      }
   }

   /* SLOT LIST */
   // Only if we're a zero slot mailbox
   if (mbox->zero_slot == 0)
   {
      // Reclaim the mail slots allocated for the mailbox so they can be reused
      slot_ptr current_slot = mbox->slot_list->head_slot;
      while (current_slot != NULL) 
      {
         // Store the next slot pointer before freeing the current slot
         slot_ptr next_slot = current_slot->next_slot;

         // Clean up the slot
         CleanSlot(current_slot, mbox);

         // Move to the next slot
         current_slot = next_slot;
      }
      free(mbox->slot_list);  // Free the slot list
      mbox->slot_list = NULL; // Set slot_list pointer to NULL to indicate it's cleaned up
   }

   /* OTHER VALUES */
   mbox->mbox_id = STATUS_UNUSED;
   mbox->status = STATUS_RELEASED;
   mbox->available_messages = STATUS_EMPTY;
   mbox->zero_slot = STATUS_UNUSED;

   return 0;
}

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Checks if at least one process is blocked on an I/O mailbox, 
             including the clock mailbox.
   Parameters - None.
   Returns - 1 if at least one process is blocked on an I/O mailbox, 0 otherwise.
   Side Effects - None.
   ----------------------------------------------------------------------- */
int check_io()
{
   // return 1 if at least one process is blocked on an I/O mailbox (including clock mbox)
   if (waiting_for_io > 0)
   {
      return 1;
   }

   return 0;   // Otherwise return 0
}

/* ------------------------------------------------------------------------
   Name - GetNextMboxID
   Purpose - Allocates a new mailbox ID by finding the next available ID.
   Parameters - None.
   Returns - The next available mailbox ID if successful, -1 if no ID is available.
   Side Effects - Increments the global variable tracking the next available mailbox ID.
   ----------------------------------------------------------------------- */
// Get the next ready mailbox ID and return it
int GetNextMboxID()
{
   int new_mbox_id = -1;                  // Initialize new mbox id to -1
   int mboxSlot = next_mbox_id % MAXMBOX; // Assign new mailbox to next_mbox_id mod MAXMBOX (to wrap around to 1, 2, 3, etc. from max)

   if (numMbox < MAXMBOX) // If there's room for another process
   {
      // Loop through until we find an empty slot
      while (MailBoxTable[mboxSlot].status != STATUS_EMPTY && mboxSlot != next_mbox_id)
      {
         next_mbox_id++;
         mboxSlot = next_mbox_id % MAXMBOX;
      }

      if (MailBoxTable[mboxSlot].status == STATUS_EMPTY)
      {
         new_mbox_id = next_mbox_id;                  // Assigns new_mbox_id to current next_mbox_id value
         next_mbox_id = (next_mbox_id + 1) % MAXMBOX; // Increment next_mbox_id for the next search
      }
   }

   return new_mbox_id;
}

/* ------------------------------------------------------------------------
   Name - GetNextSlotID
   Purpose - Allocates a new slot ID by finding the next available ID.
   Parameters - None.
   Returns - The next available slot ID if successful, -1 if no ID is available.
   Side Effects - Increments the global variable tracking the next available slot ID.
   ----------------------------------------------------------------------- */
// Get the next ready slot ID and return it
int GetNextSlotID()
{
   int new_slot_id = -1;
   int slot = next_slot_id % MAXSLOTS;
   int initialSlot = slot;
   
   if (numSlot < MAXSLOTS)  // If there's room for another slot
   {
      // Loop through until we find an empty slot or reach the initial slot
      while (MailSlotTable[slot].status != STATUS_EMPTY && slot != initialSlot)
      {
         next_slot_id = (next_slot_id + 1) % MAXSLOTS; // Increment next_slot_id for the next search
         slot = next_slot_id % MAXSLOTS;              // Update slot index
      }

      if (MailSlotTable[slot].status < 1)             // if status is unused or empty
      {
         new_slot_id = slot;   // Assigns new_slot_id to current slot
         next_slot_id = (next_slot_id + 1) % MAXSLOTS; // Increment next_slot_id for the next search
      }
   }

   return new_slot_id;
}

// AddToList functions to add an item to the end of a linked list

/* ------------------------------------------------------------------------
   Name - AddToWaitList
   Purpose - Adds a process to the waiting list of a specified mailbox.
   Parameters - int mbox_id: the ID of the mailbox.
                int status: the status to assign to the process.
                void *msg_ptr: pointer to the message (if any) associated with the process.
                int msg_size: size of the message (if any).
   Returns - 1 if the process is successfully added to the waiting list, 0 otherwise.
   Side Effects - May increase the count of waiting processes for the mailbox.
   ----------------------------------------------------------------------- */
// Add the current process to a mailbox's list of watiing processes along with its message if it's waiting to send
int AddToWaitList(int mbox_id, int status, void *msg_ptr, int msg_size)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get mailbox
   int pid = getpid();                    // Get process id - not sure how to access processes yet
   waiting_list list = mbox.waiting_list; // Get waiting list

   // Add process to mailbox's waiting list
   if (pid == NULL)
   {
      // Invalid process pointer
      return 0;
   }

   // Check if the process is already on the list
   waiting_proc_ptr current = list->pHead;
   if (current != NULL)
   {
      if (current->process->pid == pid)
      {
         // Process is already in the list
         return 0;
      }
      current = current->pNext; // Move to next process
   }

   // Allocate space for new waiting process
   waiting_proc_ptr waiting_process = (waiting_proc_ptr)malloc(sizeof(waiting_proc)); // Allocate memory for the slot

   // Assign process information
   waiting_process->process = &ProcTable[pid];
   waiting_process->process->pid = pid;
   waiting_process->process->status = status;
   waiting_process->process->delivered = -1;
   // if we're storing a message:
   if (msg_size != -1) // msg_size of -1 means no message
   {
      waiting_process->process->msg_size = msg_size;                // Store message size for later
      memcpy(waiting_process->process->message, msg_ptr, msg_size); // Copy message for later
      waiting_process->process->delivered = 0;                      // Flag for message delivered
   }

   // Update new waiting process's pointers
   waiting_process->pNext = NULL;
   waiting_process->pPrev = list->pTail;

   // Update the previous tail's next pointer to new process
   if (list->pTail != NULL)
   {
      list->pTail->pNext = waiting_process;
   }

   // New tail is the new process
   list->pTail = waiting_process;

   // If the list is empty, make new process the head
   if (list->pHead == NULL)
   {
      list->pHead = waiting_process;
   }

   // Increment the list count
   list->count++;

   // Increment number of waiting processes
   numWaitingProc++;

   return 1;
}

/* ------------------------------------------------------------------------
   Name - popWaitList
   Purpose - Removes the first process from the waiting list of a specified mailbox.
   Parameters - int mbox_id: the ID of the mailbox.
   Returns - 1 if a process is successfully removed, 0 if the waiting list is empty.
   Side Effects - Decreases the count of waiting processes for the mailbox.
   ----------------------------------------------------------------------- */
// PopList functions to pop the first item added to the linked list (head)

// Pops the head process from the waiting list and returns the waiting_proc_ptr
int popWaitList(int mbox_id)
{
   check_kernel_mode("popWaitList\n");

   // Get waiting list
   waiting_list list = MailBoxTable[mbox_id].waiting_list;

   // Check if list is empty
   if (list->count == 0)
   {
      return NULL;
   }

   // Get the oldest item and replace list's head
   waiting_proc_ptr poppedProc = list->pHead; // get the head of the list (oldest item)
   list->pHead = poppedProc->pNext;           // update the head to the next process

   // Update head/tail pointers
   if (list->pHead == NULL)
   {
      list->pTail = NULL; // If the head becomes NULL (no more items), update the tail as well
   }
   else
   {
      list->pHead->pPrev = NULL; // Update the new head's previous pointer to NULL
   }

   // Update the popped process's pointers
   if (poppedProc->pNext != NULL)
   {
      poppedProc->pNext->pPrev = NULL; // Update the next process's previous pointer to NULL
   }

   // Decrement the count of processes in the list
   list->count--;

   return 1;
}

/* ------------------------------------------------------------------------
   Name - GetNextReadySlot
   Purpose - Finds the next available slot with a message in a specified mailbox.
   Parameters - int mbox_id: the ID of the mailbox.
   Returns - Pointer to the next ready slot if successful, NULL if no slot is ready.
   Side Effects - None.
   ----------------------------------------------------------------------- */
// Get the next ready slot with message in a mailbox and clean the slot
slot_ptr GetNextReadySlot(int mbox_id)
{
   mail_box mbox = MailBoxTable[mbox_id]; // Get the mail box
   char *message = NULL;

   // Check that mail box has a slot available
   if (mbox.available_messages <= 0)
   {
      // If it doesn't, block_me and add to waiting list?
      block_me(1);
   }

   slot_ptr current = mbox.slot_list->head_slot;
   while (current != NULL)
   {
      // Iterate through each slot and check if there's an available message
      if (current->message[0] != '-1')
      {
         // if there is, set slot status to empty and return it
         current->status = STATUS_EMPTY; // slot will be cleaned outside this
         return current;                 // return the slot poibnter
      }
      current = current->next_slot; // If not, on to the next slot
   }
   return ; 
}

/* LIST INITIALIZATION FUNCTIONS */

/* ------------------------------------------------------------------------
   Name - SlotListInit
   Purpose - Initializes the slot list for a mailbox.
   Parameters - mail_box *mbox: pointer to the mailbox.
                int slots: number of slots to initialize.
                int slot_size: size of each slot.
   Returns - None.
   Side Effects - Allocates memory for and initializes the slot list of the mailbox.
   ----------------------------------------------------------------------- */
// Initializes the slot list of a mailbox but does not assign the slots yet
// Takes a pointer to the mailbox, the number and size of slots
void SlotListInit(mail_box *mbox, int slots, int slot_size)
{
   int mbox_id;

   // Allocate memory for slot_list
   mbox->slot_list = (struct slot_list *)malloc(sizeof(struct slot_list));
   memset(mbox->slot_list, 0, sizeof(slot_list)); // Initialize the slot list with 0
   mbox->slot_list->head_slot = NULL;             // Initialize head slot pointer to NULL
   mbox->slot_list->tail_slot = NULL;             // Initialize tail slot pointer to NULL
   mbox->slot_list->slot_count = slots;           // Save slot count
   mbox->slot_list->slot_size = slot_size;        // Save slot size
   mbox->slot_list->mbox_id = mbox_id;            // Save mbox ID
   mbox->slot_list->count = 0;                    // Initialize count to 0
}

/* ------------------------------------------------------------------------
   Name - WaitingListInit
   Purpose - Initializes the waiting list for a mailbox.
   Parameters - mail_box *mbox: pointer to the mailbox.
   Returns - None.
   Side Effects - Allocates memory for and initializes the waiting list of the mailbox.
   ----------------------------------------------------------------------- */
// Initialize mailbox's waiting list
void WaitingListInit(mail_box *mbox)
{
   int mbox_id;

   // Allocate memory for waiting_list
   mbox->waiting_list = (struct waiting_list *)malloc(sizeof(struct waiting_list));

   // Initialize values
   memset(mbox->waiting_list, 0, sizeof(struct waiting_list)); // Initialize the waiting list with 0s
   mbox->waiting_list->pHead = NULL;
   mbox->waiting_list->pTail = NULL;
   mbox->waiting_list->count = 0;
   mbox->waiting_list->mbox_id = mbox_id;
}

/* HANDLER FUNCTIONS */

/* ------------------------------------------------------------------------
   Name - clock_handler2
   Purpose - Handles clock interrupts, sends a message to the clock mailbox 
             every 5th interrupt.
   Parameters - int dev: device number (unused).
                void *pUnit: pointer to unit number (unused).
   Returns - None.
   Side Effects - Sends a message to the clock mailbox every 5th clock interrupt.
   ----------------------------------------------------------------------- */
// Clock handler
void clock_handler2(int dev, void *pUnit)
{
   int unit = (int)pUnit;
   int status;

   check_kernel_mode("clock handler\n");
   // Clock interrupt has occurred

   clock_count++; // Increment clock count

   if (clock_count == 5) // If this is the 5th interrupt,
   {
      clock_count = 0; // Reset clock count
      MboxCondSend(clock_mbox, &status, sizeof(int));   // Conditionally send to the clock I/O mailbox - NULL NULL?
   }

   // time slice (check if time is up, if so, make ready and dispatch)
   time_slice();
}

/* ------------------------------------------------------------------------
   Name - disk_handler
   Purpose - Handles disk interrupts, reads the disk status register and sends 
             it to the disk mailbox.
   Parameters - int dev: device number.
                void *punit: pointer to unit number.
   Returns - None.
   Side Effects - Reads the disk status register and sends it to the disk mailbox.
   ----------------------------------------------------------------------- */
// Disk handler
void disk_handler(int dev, void *punit)
{
   int status;
   int result;
   int unit = (int)punit;

   check_kernel_mode("disk handler\n");

   // Error checks is the device the correct device? Is the unit number in the correct range?
   if (unit < 0 || unit > 1)
   {
      halt(1);
   }

   // Read the device status register by using the USLOSS device_input function. You need to call this function
   device_input(DISK_DEV, unit, &status);

   // Conditionally send the content of the status register to the appropriate I/O mailbox (zero slot mailbox)
   // Cond-send is used so that low-level device handler is never blocked on the mailbox
   result = MboxCondSend(disk_mbox[unit], &status, sizeof(status));  // Need to implement disk_mbox
}

/* ------------------------------------------------------------------------
   Name - term_handler
   Purpose - Handles terminal interrupts, reads the terminal status register and sends 
             it to the terminal mailbox.
   Parameters - int dev: device number.
                void *punit: pointer to unit number.
   Returns - None.
   Side Effects - Reads the terminal status register and sends it to the terminal mailbox.
   ----------------------------------------------------------------------- */
// Terminal handler
void term_handler(int dev, void *punit)
{
   int status;
   int result;
   int unit = (int)punit;

   check_kernel_mode("terminal handler\n");

   // Error checks is the device the correct device? Is the unit number in the correct range?
   if (unit < 0 || unit > 3)
   {
      halt(1);
   }

   // Read the device status register by using the USLOSS device_input function. You need to call this function
   device_input(TERM_DEV, unit, &status);

   // Conditionally send the content of the status register to the appropriate I/O mailbox (zero slot mailbox)
   // Cond-send is used so that low-level device handler is never blocked on the mailbox
   result = MboxCondSend(term_mbox[unit], &status, sizeof(status));  // Need to implement term_mbox
}

/* ------------------------------------------------------------------------
   Name - sys_handler
   Purpose - Handles system call interrupts, calls the appropriate system call 
             handler based on the syscall number.
   Parameters - int dev: device number (unused).
                void *unit: pointer to the sysargs struct containing syscall information.
   Returns - None.
   Side Effects - Executes the appropriate system call handler.
   ----------------------------------------------------------------------- */

// Syscall Handler
void sys_handler(int dev, void *punit) {
   check_kernel_mode("sys_handler");
   sysargs *args = (sysargs*)punit;

   if (dev != SYSCALL_INT) {
      halt(1); // Only proceed if the interrupt is a syscall interrupt
   }
   // check if invalid sys number
   if (args->number >= MAXSYSCALLS)
   {
      printf("syscall_handler(): sys number %d is wrong. Halting...\n", args->number);
      halt(1);
   }
   else if (args == NULL || args->number < 0) {
      nullsys(args);
   } else
   {
      sys_vec[args->number](args);
   }
}

/* ------------------------------------------------------------------------
   Name - nullsys
   Purpose - Default system call handler for invalid syscalls. Halts USLOSS.
   Parameters - sysargs *args: pointer to the sysargs struct containing syscall information.
   Returns - None.
   Side Effects - Prints an error message and halts USLOSS.
   ----------------------------------------------------------------------- */
// nullsys for system call handler
static void nullsys(sysargs *args)
{
   if (args == NULL)
   {
      printf("nullsys(): Invalid syscall 0. Halting...\n");
   }
   else{
   printf("nullsys(): Invalid syscall %d. Halting...\n", args->number);
   }
   halt(1);
} /* nullsys */

/* ------------------------------------------------------------------------
   Name - waitdevice
   Purpose - Waits for a device of a certain type and unit to complete an operation.
   Parameters - int type: the type of the device.
                int unit: the unit number of the device.
                int *status: pointer to store the status of the operation.
   Returns - 0 if successful, -1 if the process is zapped while waiting.
   Side Effects - Blocks the calling process until the device operation completes.
   ----------------------------------------------------------------------- */
// Waitdevice
int waitdevice(int type, int unit, int *status)
{
   int result = 0;

   // Sanity checks
   // More code could be inserted before the below checking
   switch (type)
   {
   case CLOCK_DEV:
      // More code for communication with clock device
      waiting_for_io++;
      result = MboxReceive(clock_mbox, status, sizeof(int));
      break;

   case DISK_DEV:
      waiting_for_io++;
      result = MboxReceive(disk_mbox[unit], status, sizeof(int));
      break;

   case TERM_DEV:
      // More logic
      waiting_for_io++;
      result = MboxReceive(term_mbox[unit], status, sizeof(int));
      break;

   default:
      printf("waitdevice(): bad type (%d). Halting...\n", type);
      halt(1);
   }

   if (result == -3)
   {
      // we're zapped
      return -1;
   }
   else
   {
      waiting_for_io--;
      return 0;
   }
} /* Waitdevice */

/* ------------------------------------------------------------------------
   Name - CleanSlot
   Purpose - Cleans a slot in a mailbox, marking it as empty and available for reuse.
   Parameters - slot_ptr slot: pointer to the slot to clean.
                mail_box *mbox: pointer to the mailbox containing the slot.
   Returns - None.
   Side Effects - Resets the slot's status and clears its message buffer.
   ----------------------------------------------------------------------- */
void CleanSlot(slot_ptr slot, mail_box *mbox)
{
   // Clean the slot's message buffer
   memset(slot->message, 0, MAX_MESSAGE);

   // Set the slot's status and mbox ID to empty / unused
   slot->status = STATUS_EMPTY;
   slot->mbox_id = STATUS_UNUSED;

   // Decrement the count of slots in the mailbox's slot list
   mbox->slot_list->count--;

   // Fix the mailbox's slot list pointers if necessary
   // You may need to adjust head_slot and tail_slot pointers if the slot being deallocated was the head or tail of the list
   if (slot == mbox->slot_list->head_slot)
   {                                                // if slot was the head,
      mbox->slot_list->head_slot = slot->next_slot; // new head is slot's next
   }
   if (slot == mbox->slot_list->tail_slot)
   {                                                // if slot was the tail,
      mbox->slot_list->tail_slot = slot->prev_slot; // new tail is the slot's prev
   }
   if (slot->prev_slot != NULL)
   {                                                // if slot had a prev,
      slot->prev_slot->next_slot = slot->next_slot; // prev's next is slot's next
   }
   if (slot->next_slot != NULL)
   {                                                // if slot had a next,
      slot->next_slot->prev_slot = slot->prev_slot; // next's prev is slot's prev
   }

   // clean the next / prev
   slot->next_slot = NULL;
   slot->prev_slot = NULL;

   // Decrement the mailbox's count of available messages
   mbox->available_messages--;

   // Decrement global number of slots 
   numSlot--;

   // If a process is waiting on a slot, give them the slot
   // Check if there are any waiting processes
   int mbox_id = mbox->mbox_id;
   if (mbox->status == STATUS_WAIT_RELEASE || mbox->status == STATUS_RELEASED)
   {
      return 0;
   }

   if (mbox->waiting_list->count > 0)  // If there are waiting processes
   {
      if (mbox->zero_slot == 0)        // If we're not a zero slot mailbox
      {
         // If slot is available in this mailbox, allocate a slot from your mail slot table and put waiting process in
         if (mbox->slot_list->count < mbox->slot_list->slot_count) // slot is available for allocation
         {  
            waiting_proc_ptr current_proc = mbox->waiting_list->pHead;
            while (current_proc != NULL)
            {
               if (current_proc->process->message != NULL)
               {
                  
                  // Get next slot id and assign this slot to the table
                  int slot_id = GetNextSlotID();
                  slot_ptr new_slot = &MailSlotTable[slot_id];
                  // Initialize other fields
                  new_slot->mbox_id = mbox->mbox_id;
                  new_slot->status = STATUS_USED;

                  // Check message will fit into slot
                  if (mbox->slot_list->slot_size < current_proc->process->msg_size)
                  {
                     return -1;
                  }
                     // Copy the message into the slot
                     memcpy(new_slot->message, current_proc->process->message, current_proc->process->msg_size);

                     // Link the new slot to the mailbox's linked list of slots
                     new_slot->next_slot = NULL;                       // New slot's pNext is NULL
                     new_slot->prev_slot = mbox->slot_list->tail_slot; // Old tail is new slot's pPrev
                     if (mbox->slot_list->head_slot == NULL)
                     {                                         // If the list's head is NULL,
                        mbox->slot_list->head_slot = new_slot; // New slot is the new head
                     }
                     else
                     {                                                    // Otherwise,
                        mbox->slot_list->tail_slot->next_slot = new_slot; // New slot is the tail's pNext
                     }
                     mbox->slot_list->tail_slot = new_slot; // New slot is now the tail
                     mbox->slot_list->count++;              // Increment the slot list count
                     numSlot++;                             // increment global list of slots

                     // Increment the mailbox's available_messages
                     mbox->available_messages++;

                     // Remove waiting process from wait list
                     popWaitList(mbox_id);

                     // Unblock the waiting process
                     unblock_proc(current_proc->process->pid);

                     return 0;
               }
               current_proc = current_proc->pNext; // keep looking
            }
         }  
      }
   }
}

/* ------------------------------------------------------------------------
   Name - CleanWaitingProc
   Purpose - Cleans up a waiting process entry from a mailbox's waiting list.
   Parameters - waiting_proc_ptr waiting_proc: pointer to the waiting process entry.
                mail_box *mbox: pointer to the mailbox.
   Returns - None.
   Side Effects - Removes the waiting process from the mailbox's waiting list and resets its status.
   ----------------------------------------------------------------------- */
void CleanWaitingProc(waiting_proc_ptr waiting_proc, mail_box *mbox)
{
   // Clean the waiting process's message buffer
   memset(waiting_proc->process->message, 0, MAX_MESSAGE);

   // Set the waiting process's status to empty / unused
   waiting_proc->process->status = STATUS_UNUSED;

   // Decrement the count of waiting processes in the mailbox's waiting list
   mbox->waiting_list->count--;

   // Fix the mailbox's waiting list pointers if necessary
   // You may need to adjust pHead and pTail pointers if the waiting process being deallocated was the head or tail of the list
   if (waiting_proc == mbox->waiting_list->pHead)
   {                                                // if waiting process was the head,
      mbox->waiting_list->pHead = waiting_proc->pNext; // new head is waiting process's next
   }
   if (waiting_proc == mbox->waiting_list->pTail)
   {                                                // if waiting process was the tail,
      mbox->waiting_list->pTail = waiting_proc->pPrev; // new tail is the waiting process's prev
   }
   if (waiting_proc->pPrev != NULL)
   {                                                // if waiting process had a prev,
      waiting_proc->pPrev->pNext = waiting_proc->pNext; // prev's next is waiting process's next
   }
   if (waiting_proc->pNext != NULL)
   {                                                // if waiting process had a next,
      waiting_proc->pNext->pPrev = waiting_proc->pPrev; // next's prev is waiting process's prev
   }

   // clean the next / prev
   waiting_proc->pNext = NULL;
   waiting_proc->pPrev = NULL;
   waiting_proc->mbox_id = NULL;

   // Release the mbox_proc structure within the waiting_proc
   //free(waiting_proc->process);   // This caused memory issues

   // Free the memory allocated for the waiting process entry
   //free(&waiting_proc);           // This caused memory issues

   // Optionally decrement any other counters or perform additional cleanup
   numWaitingProc--;   
}