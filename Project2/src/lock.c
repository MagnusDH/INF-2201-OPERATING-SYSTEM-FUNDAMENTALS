/* lock.c
 *
 * Implementation of locks.
 */

#include "common.h"
#include "lock.h"
#include "scheduler.h"

void lock_init(lock_t *l) {
   l->status = UNLOCKED;            //Status of lock is open
   l->head = NULL;                  //Setting head to NULL, since there are no PCB's in this struct
}

void lock_acquire(lock_t *l) {
    if(l->status == LOCKED){        //If the lock is already LOCKED
        block(&l->head);            //Blocks the current_running and puts it in the "l" list
        yield();                    //Call yield() to schedule next pcb
    }

    if(l->status == UNLOCKED){      //If lock is NOT locked
        l->status = LOCKED;         //Then it is locked
    }
}

void lock_release(lock_t *l) {
    l->status = UNLOCKED;           //Lock is now UNLOCKED and open for other pcb's
    
    if(l->head != NULL){            //If locked list is empty, the the status can be UNLOCKED
        unblock(&l->head);          //Places the first element in "lock list" back into the original PCB list
    }
    yield();                        //Calling yiled to schedule the next PCB
}
