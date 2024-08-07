/*  kernel.c*/

#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "th.h"
#include "util.h"

// Statically allocate some storage for the pcb's
pcb_t pcb[NUM_TOTAL];
// Ready queue and pointer to currently running process
pcb_t *current_running;

/* This is the entry point for the kernel.
 - Initializes pcb entries
 - sets current_running pcb
 - Calls scheduler() function and start first thread/process
 */
void _start(void) 
{
	/* Declare entry_point as pointer to pointer to function returning void
	 * ENTRY_POINT is defined in kernel h as (void(**)()0xf00)
	 */
	void (**entry_point)() = ENTRY_POINT;

	// load address of kernel_entry into memory location 0xf00
	*entry_point = kernel_entry;

	
	clear_screen(0, 0, 80, 25);


	/*Initializing PCB entries*/
	int current_stack_adress = STACK_MIN + STACK_SIZE;			//Variable keeping track of allocatable stack pointer

	for(int i=0; i<NUM_TOTAL; i++){
		if(i == 0 || i == 1){									//If the PCB's are the first two ones, they are set to be PROCESS PCB's
			pcb[i].ID = i;										//Set ID to whatever i is
			pcb[i].state = STATUS_FIRST_TIME;								//Set this PCB to be in a NEW state
			pcb[i].PCB_type = "PROCESS";						//Set it to be a PROCESS PCB
			pcb[i].kernelstack_pointer = current_stack_adress;	//Setting a stack pointer
			current_stack_adress += STACK_SIZE;					//Increasing stack variable for the next stack allocation 
			pcb[i].userstack_pointer = current_stack_adress;	//Setting stack pointer
			current_stack_adress += STACK_SIZE;					//Increasing stack variable for the next stack allocation
			pcb[i].next = &pcb[i+1];							//Setting the next PCB to be i+1
			
			if(i == 0){											//If this is the first PCB in the queue
				pcb[i].previous = &pcb[NUM_TOTAL-1];			//It's previous PCB must be the last one
			}
			else{												//If not the first PCB
				pcb[i].previous = &pcb[i-1];					//Then the previous PCB is i-1
			}
		}
		else{													//If the PCB are not the first two ones, they are THREAD PCB's						
			pcb[i].ID = i;										//Set ID to whatever i is
			pcb[i].state = STATUS_FIRST_TIME;								//Set this PCB to be in a NEW state
			pcb[i].PCB_type = "THREAD";							//Set it to be a PROCESS PCB
			
			pcb[i].kernelstack_pointer = current_stack_adress;	//Setting a stack pointer
			current_stack_adress += STACK_SIZE;					//Increasing stack variable for the next stack allocation

			if(i == 8){											//If this is the last PCB
				pcb[i].next = &pcb[0];							//Then the next PCB is the first one
			}
			else{												//If not the last PCB
				pcb[i].next = &pcb[i+1];						//The next PCB i i+1
			}
 
			pcb[i].previous = &pcb[i-1];						//Previous PCB is i-1
		}
	}

	/*Assigning start addresses for each process/thread */
	pcb[0].start_address = 0x5000; 				//Plane flying
	pcb[1].start_address = 0x7000;				//"did you know that" counter
	pcb[2].start_address = (int)clock_thread;	//thread1: counting upwards
	pcb[3].start_address = (int)thread2;		//thread2, lock: counting to 100
	pcb[4].start_address = (int)thread3;		//thread3, lock: counting to 100
	pcb[5].start_address = (int)mcpi_thread0;	//thread 4-7: running
	pcb[6].start_address = (int)mcpi_thread1;	//prints: running
	pcb[7].start_address = (int)mcpi_thread2;	//prints: running
	pcb[8].start_address = (int)mcpi_thread3;	//prints running

	/*Print state of PCB's*/
	// for(int i=0; i<NUM_TOTAL; i++){
	// 	print_str(i, 0, "PCB ID:");
	// 	print_int(i, 8, pcb[i].ID);
	// 	print_int(i, 10, pcb[i].state);
	// 	print_str(i, 15, pcb[i].PCB_type);
	// 	print_hex(i, 23, pcb[i].kernelstack_pointer);
	// 	print_hex(i, 30, pcb[i].userstack_pointer);
	// 	print_int(i, 37, pcb[i].next->ID);
	// 	print_int(i, 43, pcb[i].previous->ID);
	// }

	current_running = &pcb[8];									//Setting current_running PCB to the first PCB

	scheduler();					//Calling scheduler() in scheduler.c
}

/*  Helper function for kernel_entry, in entry.S. Does the actual work
 *  of executing the specified syscall.
 */
void kernel_entry_helper(int fn)
{
}


/*
Når en prosess og en thread kjører, lagrer de dataene sine i ......stack
	-Alle registre pushes på denne stacken
Når data for en prosess skal lagres når en context switch skjer så lagres disse i .....stack
	-Stack pointer lagres her?


	-push regisdttre poå kernelstack
	-ikke gjør en dritt på userstack, bare bytt til kernelstack
*/

/*
-En thread startes og kjører
	-Den aquirer en lock, og locken blir låst
	-Den kjører til den yielder
	-Hvis da en annen thread prøver å kjøre mens locken er låst, så må den bli BLOCKED
	-Når threaden som tok låsen kjører igjen og yielder() på riktig plass, så releases locken
	
-en prosess/thread kjører
-
*/