/* kernel.h
 *
 * Various definitions used by the kernel and related code.
 */
#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

/* Cast 0xf00 into pointer to pointer to function returning void
 * ENTRY_POINT is used in syslib.c to declare 'entry_point'
 */
#define ENTRY_POINT ((void (**)())0xf00)

// Constants
enum
{
	/* Number of threads and processes initially started by the kernel. Change
	 * this when adding to or removing elements from the start_addr array.
	 */
	NUM_THREADS = 7,
	NUM_PROCS = 2,
	NUM_TOTAL = (NUM_PROCS + NUM_THREADS),

	SYSCALL_YIELD = 0,
	SYSCALL_EXIT,
	SYSCALL_COUNT,

	//  Stack constants
	STACK_MIN = 0x10000,
	STACK_MAX = 0x20000,
	STACK_OFFSET = 0x0ffc,
	STACK_SIZE = 0x1000
};

/* The process control block is used for storing various information about
 *  a thread or process
 */
typedef struct pcb_t {
	int ID;					//Identification for a PCB
	int state;				//Which state the PCB is in. (New, Ready, Running, Waiting, Executing, Suspended, Terminated)
	char *PCB_type;			//Process PCB or Thread PCB
	int kernelstack_pointer;//Where the content for a process or thread is stashed.
	int userstack_pointer;	//Where the content for a process or thread is stashed.
	int start_address;		//Pointer to where this thread/process starts
	struct pcb_t *next; 	//Used when job is in the ready queue
	struct pcb_t *previous;	//Pointer to previous PCB
} pcb_t;

// The currently running process, and also a pointer to the ready queue
extern pcb_t *current_running;

// Prototypes
void kernel_entry(int fn);
void kernel_entry_helper(int fn);
#endif
