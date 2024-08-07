# bootblock.s

# .equ symbol, expression
# These directive set the value of the symbol to the expression
  .equ    BOOT_SEGMENT,      0x07c0   # Start of boot block
  .equ    DISPLAY_SEGMENT,   0xb800   #
  .equ    KERNEL_SEGMENT,    0x0000   # Start of Kernel -Relative to start of boot block???
  .equ    KERNEL_OFFSET,     0x1000   # Size of Kernel???

# You need to decide where to put the stack
  .equ    STACK_POINTER,     0x07e01  # Stack starts at this address (Points to current space)
  .equ    STACK_SEGMENT,     0x9fbff  # Stack ends at this address  (Bottom of the stack)

.text                   # Code segment
.globl    _start        # The entry point must be global
.code16                 # Real mode
.org 0x0

#
# The first instruction to execute in a program is called the entry
# point. The linker expects to find the entry point in the "symbol" _start
# (with underscore).
#

_start:                     
  jmp beyondReservedSpace     # Jumps to the starting point

kernelSize:
  .word 0                     # bootimage will write size of kernel, in sectors


beyondReservedSpace:          # Program starts here, doing some initialization (Pre code)
  movw $DISPLAY_SEGMENT, %bx  # Moving Display_segment to general purpose register "bx"
  movw %bx, %es               # Moving Display_segment to the "Extra data Segment" register
  
  # Clear screen
  movw $0x0a00, %ax           # Fill with black background / green foreground
  movw $2000, %cx             # Number of characters (80x25 screen in text mode = 2000)
  xorw %di, %di               # DI = 0
  rep stosw
  
  movb $0x4b, %es:(0x0)       # Write 'K' in the upper left corner of the screen


# ------------------------------ My code ------------------------------
  # Setting up the stack
  movw $STACK_SEGMENT, %ax    # Moving "end-address" of stack to a general purpose register "ax"
  movw %ax, %ss               # Moving "end-address" of stack to the "Stack Segment" register
  movw $STACK_POINTER, %sp    # Moving "stack pointer" to "Stack Pointer" register

  # Moving Boot block to the Data Segment register
  movw $BOOT_SEGMENT, %ax     # Moving start of Boot block to ax register
  movw %ax, %ds               # Moving start of boot block to "Data Segment" register

  jmp INT13                   # Jumping to INT13 call setup

# Preparing registers for INT13 interupt call
INT13:
  movb $2, %ah                # Value 02 is pre defined so that the Iterupt13 call can "Read Sectors From Drive"
  movb (kernelSize), %al      # Move the number of sectors to read into %al register
  movb $0, %ch                # Move track number 0 into %ch register
  movb $2, %cl                # Move sector number 2 into %cl register
  movb $0, %dh                # Move head number 0 into %dh register
  movb $0, %dl                # Move drive number 0 to %dl register
  
  movw $KERNEL_SEGMENT, %bx
  movw %bx, %es               # Move start of Kernel into %es register
  movw $KERNEL_OFFSET, %bx    # Move size of kernel into %bx register (kernel is located at ES:BS)

  int $0x13                   # Make the INTERUPT13 call

  movw $KERNEL_SEGMENT, %bx
  movw %bx, %ds               # Setting up Kernel_segment
  
  ljmp $KERNEL_SEGMENT, $KERNEL_OFFSET  # Jump to the Kernel address and make it take controll