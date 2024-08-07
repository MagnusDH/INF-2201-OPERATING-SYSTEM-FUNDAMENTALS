/*  scheduler.c
 */
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "util.h"
#include "lock.h"

int context_start;
int context_end;

/* Call scheduler to run the 'next' process*/
void yield(void) {
    /*
    OBS!: 
    Calling scheduler_entry() instead of scheduler() will cause Bochs to crash half way through....
    */
    // scheduler_entry();
    scheduler();
}

/* The scheduler picks the next job to run, and removes blocked and exited
 * processes from the ready queue, before it calls dispatch to start the
 * picked process.*/

 /*Scheduler picks the next PCB to run before calling dispatch()*/
void scheduler(void) {
    context_start = get_timer();
    current_running = current_running->next;
    while(current_running->state == STATUS_EXITED){ //If the current_running is in an EXITED state
        current_running = current_running->next;    //Then update the current running to the next PCB
    }

    //Current_running is not EXITED
    dispatch();                                      //Call dispatch to run the current_running
}

/* dispatch() does not restore gpr's it just pops down the kernel_stack,
 * and returns to whatever called scheduler (which happens to be scheduler_entry,
 * in entry.S).
 */
void dispatch(void) {
    //Its responsibilities is handling essentially two cases; has the process been run before or is it new?
    if(current_running->PCB_type == "PROCESS"){                 //If this is a process PCB
        if(current_running->state == STATUS_FIRST_TIME){        //If this is the first time the process is being run
            current_running->state = STATUS_READY;              //It is no longer at the start of its running process, it has run before

            asm volatile(							            //Defining this to write assembly code	
                "movl current_running, %eax;"                   //Moving start address of current_running process into %eax
                "movl 12(%eax), %esp;"                          //Moving kernelstack_pointer into %esp, to go to correct place in stack
                "jmp 20(%eax);"                                 //Jumping to address specified in the current PCB
            );
        }
        else if(current_running->state == STATUS_READY){        //If this is not the first time this Process is being run
            asm volatile(                                       //Defining this to write assembly code
                "movl current_running, %eax;"                   //Moving start address of current_running Process to %eax
                "movl 12(%eax), %esp;"                          //Restoring kernelstack_pointer to correct place
                "frstor (%esp);"                                //Restoring floating point registers
                "add $108, %esp;"                               //Restoring stack to its original size
                "popfl;"                                        //Popping back all floating point registers back to registers 
                "popal;"                                        //Popping of all arguments from stack back into registers
                "ret;"
            );
            context_end = get_timer();
            /*
            OBS, trying to implement context switch measurement
            */
            // print_str(16,0, "Context switch time = ");
            // print_int(16, 30, context_end - context_start);
            // asm volatile(
            //     "ret;"                                          //Returning to where last process stopped
            // );
        }
    }
    else if(current_running->PCB_type == "THREAD"){             //If this is a thread PCB
        if(current_running->state != STATUS_BLOCKED)            //If this PCB is NOT blocked
            if(current_running->state == STATUS_FIRST_TIME){    //If this is the first time this thread is being run
                asm volatile(							        //Defining this to write assembly code	
                    "movl current_running, %eax;"               //Moving start address of current_running process into %eax
                    "movl 12(%eax), %esp;"                      //Moving kernelstack_pointer into %esp, to go to correct place in stack
                    "jmp 20(%eax);"                             //Jumping to address specified in the current PCB
                );
            }
            else if(current_running->state == STATUS_READY){    //If this thread has run before
                asm volatile(                                   //Defining this to write assembly code
                    "movl current_running, %eax;"               //Moving start address of current_running Process to %eax
                    "movl 12(%eax), %esp;"                      //Restoring kernelstack_pointer to correct place
                    "frstor (%esp);"                            //Restoring floating point registers
                    "add $108, %esp;"                           //Restoring stack to its original size
                    "popfl;"                                    //Popping back all floating point registers back to registers 
                    "popal;"                                    //Popping of all arguments from stack back into registers
                    "ret;"
                );
                context_end = get_timer();
                /*
                OBS, trying to implement context switch measurement
                */
                // print_str(16,0, "Context switch time = ");
                // print_int(16, 30, context_end - context_start);
                // asm volatile(
                //     "ret;"                                          //Returning to where last process stopped
                // );    
            }

        //if current_running->state == STATUS BLOCKED, then it is located in the "locked" queue and won't be scheduled
    }    
}

/* Remove the current_running process from the linked list so it
 * will not be scheduled in the future*/
void exit(void) {
    current_running->state = STATUS_EXITED;     //Status of this PCB is not EXITED (does no longer exist)
    current_running->previous->next = current_running->next;         //Removing current_running from original list by linking its previous and next togheter
    current_running->next->previous = current_running->previous;     //Updating its next pointer to point to its previous as well
    scheduler();                                //Call scheduler to schedule the next PCB to run
}

/* 'q' is a pointer to the waiting list where current_running should be
 * inserted */
void block(pcb_t **q) {
    pcb_t *l = (*q);                                                    //Used for putting a given PCB into a lock list ((*q) is the head pcb found in lock_t "l")

    current_running->state = STATUS_BLOCKED;                            //Setting current_running->state to BLOCKED

    if(l == NULL){                                                      //If the given list is empty
        l = current_running;                                            //Set current_running to be first element in list
        
        current_running->previous->next = current_running->next;        //Removing current_running from original list by linking its previous and next togheter
        current_running->next->previous = current_running->previous;    //Updating its next pointer to point to its previous as well
    }
    else if(l != NULL){                                                 //If list is not empty
        pcb_t *tmp = l;                                                 //Tmp = head address
        while(tmp->next != NULL){                                       //While we have not reached the end of the list
            tmp = tmp->next;                                            //tmp next is incremented
        }
        //Have reached the end of the "lock" list and found where to insert current_running
        tmp->next = current_running;                                    //Found the end of the list, and inserting the PCB 
        current_running->previous->next = current_running->next;        //Removing the CURRENT PCB from the original list of PCB's and linking the rest of them togheter
        current_running->next->previous = current_running->previous;    //Updating its next pointer to point to its previous as well
    }
}

/* Must be called within a critical section.
 * Unblocks the first process in the waiting queue (q), (*q) points to the
 * last process. */
void unblock(pcb_t **q)
{
    pcb_t *l = (*q);                        //(*q) gives access to start of locked list head
    pcb_t *tmp = l;                         //tmp is going to be placed back in original list
    print_str(15, 0, "PCB to be removed:");
    print_int(15, 20, l->ID);
    
    l = l->next;                            //Updating head to be the next, so that we forget the original head
    print_str(16, 0, "l->next ID:");
    print_int(16, 20, l->ID);
    
    current_running->next->previous = tmp;  //Linking current_running->next's previous pointer to be tmp    
    tmp->previous = current_running;        //Linking tmp->previous to the current_running
    tmp->next = current_running->next;      //Linking tmp->net to be the one after current_running    
    current_running->next = tmp;            //Linking current_running->next to be tmp
}