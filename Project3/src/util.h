/*
 * Various utility functions that can be linked with both the kernel
 * and user code.
 */
#ifndef UTIL_H
#define UTIL_H

#include "common.h"

void clear_screen(int minx, int miny, int maxx, int maxy);

/* scroll screen */
void scroll(int minx, int miny, int maxx, int maxy);

/* Read the character stored at location (x,y) on the screen */
int peek_screen(int x, int y);

/* Wait for atleast <msecs> number of milliseconds. Does not handle
 * the counter overflowing.*/
void ms_delay(uint32_t msecs);

/* Read the pentium time stamp counter */
uint64_t get_timer(void);

/* Convert an ASCII string (like "234") to an integer */
uint32_t atoi(char *s);

/* Convert an integer to an ASCII string, Page 64 */
void itoa(uint32_t n, char *s);

/* Convert an integer to an ASCII string, base 16 */
void itohex(uint32_t n, char *s);

/* Write a character on the screen */
void print_char(int line, int col, char c);

/* Write an integer to the screen */
void print_int(int line, int col, int num);

/* Write an integer to the screen, base 16 */
void print_hex(int line, int col, uint32_t num);

/* Write a string to the screen */
void print_str(int line, int col, char *str);

/* Reverse a string, Page 62 */
void reverse(char *s);

/* Get the length of a null-terminated string, Page 99 */
int strlen(char *s);

/* return TRUE if string s1 and string s2 are equal */
int same_string(char *s1, char *s2);

/* Block copy: copy size bytes from source to dest.
 * The arrays can overlap*/
void bcopy(char *source, char *destin, int size);

/* Zero out size bytes starting at area */
void bzero(char *a, int size);

/* Read byte from I/O address space */
uint8_t inb(int port);

/* Write byte to I/O address space */
void outb(int port, uint8_t data);

void srand(uint32_t seed);

/* Return a pseudo random number */
uint32_t rand(void);

#endif /* !UTIL_H */
