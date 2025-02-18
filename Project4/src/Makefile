# Makefile for the OS projects.
CC = gcc
LD = ld

ETAGS = etags
TAGS = tags

KERNEL_LOCATION    = 0x8000 # physical & virtual address of kernel
PROCESS_LOCATION   = 0x1000000 # virtual address of processes

# Compiler flags
CCOPTS = -Wall -Wextra -Wno-unused -c -m32 -O2 -fno-stack-protector -fno-builtin -fno-unit-at-a-time -fno-toplevel-reorder -fno-defer-pop \
         -mfpmath=387 -march=i386 -mno-mmx -mno-sse -mno-sse2 \
         -DPROCESS_START=$(PROCESS_LOCATION)

# Linker flags
LDOPTS = -znorelro -nostdlib -nostartfiles -melf_i386

# Add your user program here:
USER_PROGRAMS	= process1.o

KERNEL	=	kernel.o

# Common objects used by both the kernel and user processes
COMMON		=	util.o print.o
# Processes to create
PROCESSES	=	shell.o process1.o process2.o process3.o process4.o

# USB subsystem
USB = usb/pci.o usb/uhci_pci.o usb/uhci.o usb/ehci_pci.o usb/usb_hub.o \
			usb/usb.o usb/usb_msd.o usb/scsi.o usb/usb_hid.o usb/usb_keyboard.o \
			usb/allocator.o

# Objects needed by the kernel
COMMON_KERNEL	= entry.o $(COMMON) th1.o th2.o scheduler.o \
			dispatch.o interrupt.o memory.o paging.o sleep.o \
			time.o $(USB)

KERNELOBJ	=	$(COMMON_KERNEL) thread.o mbox.o keyboard.o

# Allow user to build kernel from given object files as well
IPC_GIVEN	=	given/kernel.o thread.o
PM_GIVEN	=	given/thread.o given/mbox.o given/keyboard.o
INT_GIVEN	=	given/kernel.o given/mbox.o given/keyboard.o

IPC_KERNELOBJ	=	$(COMMON_KERNEL) $(IPC_GIVEN) given/keyboard.o mbox.o
PM_KERNELOBJ	=	$(COMMON_KERNEL) $(PM_GIVEN)
INT_KERNELOBJ	=	$(COMMON_KERNEL) $(INT_GIVEN) thread.o


# Objects needed to build a process
PROCOBJ		=       $(COMMON) syslib.o

# Targets that aren't files (phony targets)
.PHONY: all ipc pm int ipc_kernel pm_kernel int_kernel boot demo \
	progdisk depend clean distclean ipc_image pm_image int_image bootblock bootblock_nokernel boot

# Makefile targets
all: createimage kernel bootblock image $(PROCESSES:.o=)

ipc: createimage ipc_kernel bootblock_nokernel ipc_image $(PROCESSES:.o=)

pm: createimage pm_kernel bootblock_nokernel pm_image $(PROCESSES:.o=)

int: createimage int_kernel bootblock_nokernel int_image $(PROCESSES:.o=)

kernel: $(KERNELOBJ) $(KERNEL)
	$(LD) $(LDOPTS) -Ttext $(KERNEL_LOCATION) -o $@ $^
	objcopy $@ $@ -G kernel_start

entry.o: entry.S
	$(CC) $(CCOPTS) -x assembler-with-cpp -c $< -o $@

# Build a kernel with the given IPC object files
ipc_kernel: $(IPC_KERNELOBJ)
	$(LD) $(LDOPTS) -Ttext $(KERNEL_LOCATION) -o kernel $^ 

# Build a kernel using the given PM object files
pm_kernel: $(PM_KERNELOBJ) $(KERNEL)  
	$(LD) $(LDOPTS) -Ttext $(KERNEL_LOCATION) -o kernel $^

# Build a kernel using the given INT object files
int_kernel: $(INT_KERNELOBJ)
	$(LD) $(LDOPTS) -Ttext $(KERNEL_LOCATION) -o kernel $^

# Build the processes
process1: proc_start.o process1.o $(PROCOBJ)
	$(LD) $(LDOPTS) -Ttext $(PROCESS_LOCATION) -o $@ $^

process2: proc_start.o process2.o $(PROCOBJ)
	$(LD) $(LDOPTS) -Ttext $(PROCESS_LOCATION) -o $@ $^

process3: proc_start.o process3.o $(PROCOBJ)
	$(LD) $(LDOPTS) -Ttext $(PROCESS_LOCATION) -o $@ $^

process4: proc_start.o process4.o $(PROCOBJ)
	$(LD) $(LDOPTS) -Ttext $(PROCESS_LOCATION) -o $@ $^

shell: proc_start.o shell.o $(PROCOBJ)
	$(LD) $(LDOPTS) -Ttext $(PROCESS_LOCATION) -o $@ $^

createimage: createimage.c
	$(CC) -o $@ $<	

asmsyms.h: asmdefs
	./$< > $@

asmdefs: asmdefs.c
	$(CC) -o $@ $<

bootblock.o: bootblock.S
	$(CC) $(CCOPTS) -DKERNEL_LOAD_ADDR=$(KERNEL_LOCATION) -DKERNEL_ADDR=0x$(shell nm kernel | grep "T kernel_start" | cut -c-8) -x assembler-with-cpp -o $@ $<

bootblock_nokernel: bootblock.o
	$(LD) $(LDOPTS) -Ttext 0x0 -o bootblock $<

bootblock: kernel bootblock_nokernel

cantboot.o: cantboot.s
	$(CC) $(CCOPTS) $<

cantboot: cantboot.o kernel 
	$(LD) $(LDOPTS) -Ttext 0x0 -o $@ $<

# Create an image to put on the usb
image: createimage kernel $(PROCESSES:.o=)
	strip --remove-section=.note.gnu.property $^
	./createimage --extended --vm --kernel ./bootblock ./kernel $(PROCESSES:.o=)

boot: createimage kernel bootblock image $(PROCESSES:.o=)
ifneq ($(FLOPPYDEV),)
	dd if=image of=$(FLOPPYDEV) bs=512
else
	@echo -e "Cannot seem to find a usable floppy drive. If you think you\
have a usable\ndevice that isn't detected, try this:\
'make FLOPPYDEV=/dev/somedevice boot'"
endif
# Create an image to put on the usb
ipc_image: createimage ipc_kernel $(PROCESSES:.o=)
	./createimage --extended --vm --kernel ./bootblock ./kernel $(PROCESSES:.o=)

# Create an image to put on the usb
pm_image: createimage pm_kernel $(PROCESSES:.o=)
	./createimage --extended --vm --kernel ./bootblock ./kernel $(PROCESSES:.o=)

# Create an image to put on the usb
int_image: createimage int_kernel $(PROCESSES:.o=)
	./createimage --extended --vm --kernel ./bootblock ./kernel $(PROCESSES:.o=)


# Figure out dependencies, and store them in the hidden file .depend
depend: .depend
.depend: *.[cSh]
	$(CC) $(CCOPTS) -MM -MG *.[cS] > $@

# Clean up!
clean:
	-$(RM) *.o
	-$(RM) usb/*.o
	-$(RM) asmsyms.h
	-$(RM) $(PROCESSES:.o=) kernel image createimage bootblock asmdefs
	-$(RM) .depend


# No, really, clean up!
distclean: clean
	-$(RM) *~
	-$(RM) \#*
	-$(RM) *.bak
	-$(RM) TAGS tags
	-$(RM) bochsout.txt
	-$(RM) serial.out

# Make tags for emacs
TAGS:	*.[chsS]
	$(ETAGS) $^

# Make tags for vi
tags:	*.[chsS]
	$(TAGS) $^

# How to compile a C file
%.o:%.c
	$(CC) $(CCOPTS) $< -o $@

# How to assemble
%.o:%.s
	$(CC) $(CCOPTS) $<

%.o:%.S
	$(CC) $(CCOPTS) -x assembler-with-cpp $< 

# How to produce assembler input from a C file
%.s:%.c
	$(CC) $(CCOPTS) -S $<

# Include dependencies
# (the leading dash prevents warnings if the file doesn't exist)
-include .depend
