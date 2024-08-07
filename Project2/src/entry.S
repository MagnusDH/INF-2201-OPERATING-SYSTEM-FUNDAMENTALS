#	entry.S
#	This file contains the code for kernel_entry, as well as
#	scheduler_entry, which saves the context of the current process
#	before calling the scheduler. The file is pre-processed before it
#	is assembled, allowing (among other things) the use of macros
#	and C-style comments.

.data
	.align 4
.text
.code32
	.align 4

# Make symbols visible for ld
.globl	kernel_entry
.globl	scheduler_entry


# void kernel_entry(int fn)
#
# SYSCALL(i) in syslib.c calls this function
# fn = system call number (defined in kernel.h)
#
ke_scratch:
	.long	0
ke_arg:
	.long	0
kernel_entry:
	# Save eax
	movl	%eax, (ke_scratch)
	
	# Save argument
	movl	4(%esp), %eax
	movl	%eax, (ke_arg)

	# Now it's up to you!
	jmp scheduler_entry		# Jumping to scheduler_entry

#	This function gets called to enter the scheduler, saving registers before doing so.
scheduler_entry:

	pushal							# Copying all registers to stack for the current running process/thread
	pushfl							# Copying all floating point registers to stack (flag registers)
	
	sub $108, %esp					# making more space on stack
	fsave (%esp)					# saving floating point register


	movl (current_running), %eax	# Moving address of current_running PCB to %eax
	movl %esp, 12(%eax)				# Saving current userstack_pointer to the current_running pcb->userstack_pointer
	
	call scheduler					# Calling scheduler() function in scheduler.c