/* syslib.h
 */
#ifndef SYSLIB_H
#define SYSLIB_H

/*Pointer to pointer to function with non-defined argument list returning void at address 0xf00*/
#define ENTRY_POINT ((void (**)()) 0xf00)

// Prototypes for exported system calls
void yield(void);
void exit(void);

#endif
