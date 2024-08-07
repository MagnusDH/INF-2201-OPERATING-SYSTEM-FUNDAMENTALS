/*
 * memory.c
 * Note:
 * There is no separate swap area. When a data page is swapped out,
 * it is stored in the location it was loaded from in the process'
 * image. This means it's impossible to start two processes from the
 * same image without screwing up the running. It also means the
 * disk image is read once. And that we cannot use the program disk.
 *
 * Best viewed with tabs set to 4 spaces.
 */

#include "common.h"
#include "interrupt.h"
#include "kernel.h"
#include "memory.h"
#include "scheduler.h"
#include "thread.h"
#include "tlb.h"
#include "usb/scsi.h"
#include "util.h"



/* Use virtual address to get index in page directory.  */
inline uint32_t get_directory_index(uint32_t vaddr);

/*
 * Use virtual address to get index in a page table.  The bits are
 * masked, so we essentially get a modulo 1024 index.  The selection
 * of which page table to index into is done with
 * get_directory_index().
 */
inline uint32_t get_table_index(uint32_t vaddr);


/* Debug-function. 
 * Write all memory addresses and values by with 4 byte increment to output-file.
 * Output-file name is specified in bochsrc-file by line:
 * com1: enabled=1, mode=file, dev=serial.out
 * where 'dev=' is output-file. 
 * Output-file can be changed, but will by default be in 'serial.out'.
 * 
 * Arguments
 * title:		prefix for memory-dump
 * start:		memory address
 * end:			memory address
 * inclzero:	binary; skip address and values where values are zero
 */
static void rsprintf_memory(char *title, uint32_t start, uint32_t end, uint32_t inclzero)
{
	uint32_t numpage, paddr;
	char *header;

	rsprintf("%s\n", title);

	numpage = 0;
	header = "========================== PAGE NUMBER %02d ==========================\n";

	for(paddr = start; paddr < end; paddr += sizeof(uint32_t)) {

		/* Print header if address is page-aligned. */
		if(paddr % PAGE_SIZE == 0) {
			rsprintf(header, numpage);
			numpage++;
		}
		/* Avoid printing address entries with no value. */
		if(	!inclzero && *(uint32_t*)paddr == 0x0) {
			continue;
		}
		/* Print: 
		 * Entry-number from current page. 
		 * Physical main memory address. 
		 * Value at address.
		 */
		rsprintf("%04d - Memory Loc: 0x%08x ~~~~~ Mem Val: 0x%08x\n",
					((paddr - start) / sizeof(uint32_t)) % PAGE_N_ENTRIES,
					paddr,
					*(uint32_t*)paddr );
	}
}




static int memory_lock;			//Used for synchronization
lock_t paging_lock;				//Used for synchronization
uint32_t next_free_memory;		//keeps track of where the next free memory is
uint32_t *kernel_page_directory;//Pointer to kernel's page directory
uint32_t *kernel_pagetable;		//Used to connect to process page_directory

/*
-Allocates a given number of bytes of physical memory
-Returns an integer address to the allocated memory
-Note: if size < 4096 bytes, then 4096 bytes are used, beacuse the
 memory blocks must be aligned to a page boundary.
*/
uint32_t allocate_memory(uint32_t num_bytes) {
	
	//spinlock_acquire(&memory_lock);					//Spinlock to do memory allocation privately

	uint32_t memory = next_free_memory;
	next_free_memory += num_bytes;					//Increment next free memory location to point to next free location

	//If the next_free_memory is not at aligned with 4096 bytes
	if( (next_free_memory & 0xfff) != 0) {
		// align next_free_memory to fit page sizes
		next_free_memory = (next_free_memory & 0xfffff000) + 0x1000;
	}

	//spinlock_release(&memory_lock);
	return memory;
}


/*
-Creates a page where all 1024 entries are set to zero.
-Returns a uint32_t pointer to a freshly allocated page in physical memory.
*/
uint32_t *create_page()
{
	//Allocate memory for page
	uint32_t *page = (uint32_t *)allocate_memory(PAGE_SIZE);

	//Set all entries in page to zero
	for(int i=0; i<1024; i++){
		page[i] = 0;
	}

	return page;
}



/*
-Places an address at index in table.
-The address consist of 20 msb as the physical address and 12 lsb as entry bits set by user
*/
void set_table_entry_bits(uint32_t *table, uint32_t index, uint32_t physical_address, int entry_bits)
{
	//Mask out the last 12 bits of physical address so that entry bits can be set
	physical_address = physical_address & PE_BASE_ADDR_MASK;

	//Set wanted entry bits to physical address
	physical_address = physical_address | entry_bits;

	//Place physical address at special index in table
	table[index] = physical_address;
}


/*
-Places a given address at index in table.
-The address consist of 20 msb as the physical address and 12 lsb as entry bits set by user
-The address is placed at the index decided by the virtual address.
*/
void set_directory_entry_bits(uint32_t *directory, uint32_t virtual_address, uint32_t physical_address, int entry_bits)
{
	//Gets the directory index where physical address will be placed
	uint32_t index = get_directory_index(virtual_address);

	//Mask out the last 12 bits of physical address so that entry bits can be set
	physical_address = physical_address & PE_BASE_ADDR_MASK;

	//Set wanted entry bits to physical address
	physical_address = physical_address | entry_bits;

	//Place physical address at special index in directory
	directory[index] = physical_address;
}


/*
-Inserts the given page into the given table.
-Virtual_address is used to find table index where the page will be placed.
-The 20 msb of page will be inserted into table as physical address
-the 12 lsb (Entry_bits) are set to whatever the user wants.
*/
void insert_page(uint32_t *page, uint32_t *table, uint32_t virtual_address, int entry_bits)
{
	//Get table index where page will be placed
	uint32_t index = get_table_index(virtual_address);

	//Convert page address to uint32_t
	uint32_t address = (uint32_t)page;
	
	//Zero out the last 12 bits so that entry bits can be set
	address = address & PE_BASE_ADDR_MASK;

	//Set wanted entry bits at the end of page address
	address = address | entry_bits;

	//Place page address at special index in table (20 physical bits, 12 entry bits)
	table[index] = address;
}


/*
-Inserts the given table into the given directory.
-Virtual_address is used to find directory index where table will be placed.
-Entry_bits sets the last 12 bits to whatever the user wants.
*/
void insert_table(uint32_t *table, uint32_t *directory, uint32_t index, int entry_bits)
{
	//Convert table address to uint32_t
	uint32_t address = (uint32_t)table;
	
	//Zero out the last 12 bits so that entry bits can be set
	address = address & PE_BASE_ADDR_MASK;

	//Set wanted entry bits at the end of table address
	address = address | entry_bits;

	//Place table address at special index in directory (20 physical bits, 12 entry bits)
	directory[index] = address;
}


/*DONE
-Creates a page for the kernels page directory
-Creates a page_table for the kernel and adds it to kernel_directory
*/
void create_kernel_pagedirectory()
{
	//Creates a page_directory where all 1024 entries are zeroed out
	kernel_page_directory = create_page();
	
	//Create page_table where all 1024 entries are zeroed out
	kernel_pagetable = create_page();

	//Mark all existing pages in kernel_table as "present"
	for(int addr=0; addr<MAX_PHYSICAL_MEMORY; addr+= PAGE_SIZE){
		uint32_t index = get_table_index(addr);
		set_table_entry_bits(kernel_pagetable, index, addr, 3);
	}


	//Place the video memory address in kernel_pagetable at index 184 and mark page as present
	uint32_t screen_index = get_table_index((uint32_t)SCREEN_ADDR);
	set_table_entry_bits(kernel_pagetable, screen_index ,(uint32_t)SCREEN_ADDR, 7);

	//Insert kernel_pagetable into kernel_page_directory (index 0) and mark table as present	
	insert_table(kernel_pagetable, kernel_page_directory, 0, 3);
}

/*
-init_memory()
-called once by _start() in kernel.c
-You need to set up the virtual memory map for the kernel here.
*/
void init_memory(void) 
{
	spinlock_init(&memory_lock);
	lock_init(&paging_lock);
	next_free_memory = MEM_START;

	//Initialize 33 pages
	for(int i=0; i<PAGEABLE_PAGES; i++){
		// pages[i].page_address = create_page();
		pages[i].page_size = PAGE_SIZE;
		pages[i].pinned = 0;
		pages[i].modified = 0;

		//Set all entries in page to 0
		for(int j=0; j<1024; j++){
			pages[i].buffer[j] = 0;
		}		
	}

	/*Creates page_table and page_directory for kernel.
	Kernels page_table is then inserted into kernels page_directory*/ 
	create_kernel_pagedirectory();
	
}


/*
-Sets up a page directory and page table for a new process or thread.

-Creates this hierarchy:
	-Process_directory:
		-index[0] = kernel_table
			index[0-288] = pointers to pages
		-index[4] = process_code_table
		-index[959] = process_stack_table
			-index[0] = process_stack_page
*/
void setup_page_table(pcb_t *pcb)
{	
	lock_acquire(&paging_lock);

	/*HANDLE THREADS*/

	//Threads use the kernels page_directory. So set a pointer to that and return.
	if(pcb->is_thread == 1){
		pcb->page_directory = kernel_page_directory;
		lock_release(&paging_lock);
		return;
	}
	

	/*HANDLE PROCESSES*/

	if(pcb->is_thread == 0){
		//Allocate a page for process_directory
		uint32_t *process_page_directory = create_page();
		// process_page_directory[0] = 0x10000000;

		//Insert created kernel_pagetable into process_directory
		insert_table(kernel_pagetable, process_page_directory, 0, 0x7);

		//Create page_table for process_code and insert it into page_directory
		uint32_t *process_code_pagetable = create_page();
		// process_code_pagetable[0] = 0x10000000;
		//Process virtual address start is at 0x1000000
		uint32_t process_start_index = get_directory_index(0x1000000);
		insert_table(process_code_pagetable, process_page_directory, process_start_index, 0x7); //present, read/write, user bits = 1

		
		//Create page_table for process_stack and insert it into page_directory
		uint32_t *process_stack_table = create_page();
		// process_stack_table[0] = 0x10000000;
		uint32_t process_stack_index = get_directory_index(PROCESS_STACK);
		insert_table(process_stack_table, process_page_directory, process_stack_index, 0x7); //read/write, user bits = 1


		//Create stack page and insert it to process_stack_pagetable
		uint32_t *stack_page = create_page();
		// stack_page[0] = 0x10000000;
		uint32_t index = get_table_index(PROCESS_STACK);
		insert_page(stack_page, process_stack_table, index, 0x7);

		pcb->page_directory = process_page_directory;
	}
		
	lock_release(&paging_lock);
}




//Used to choose which page that will be evicted
int evict_count = 1;

/*
-called by exception_14 in interrupt.c (the faulting address gets stored in
-current_running->fault_addr)
-Interrupts are on when calling this function.
*/
void page_fault_handler(void)
{ 
	lock_acquire(&paging_lock);

	//Increment process->page_fault_count
	current_running->page_fault_count++;
	
	//If error code results in page fault or protection fault
	if((current_running->error_code == 4) | (current_running->error_code == 5)){


		//If there is available memory to create a new page
		if(next_free_memory <= MAX_PHYSICAL_MEMORY - PAGE_SIZE){
			
			//Crate new page to write missing code to
			uint32_t *new_page = create_page();
			
			//Create buffer to write missing code into
			char buffer[PAGE_SIZE];
			
			//Find which block offset the virtual address corresponds to
			int offset = (current_running->fault_addr & PE_BASE_ADDR_MASK) - current_running->start_pc;
			offset = (offset / SECTOR_SIZE);
			
			int num_sectors = 0;

			//Find how many sectors remaining that should be read
			if(current_running->swap_size - offset >= 8){
				num_sectors = 8;
			}
			
			else if(current_running->swap_size - offset < 8){
				num_sectors = current_running->swap_size - offset;
			}

			//Read missing code from disk into buffer with offset of faulting address
			scsi_read(current_running->swap_loc+offset, num_sectors, (char *)buffer);


			//Copy buffer into new_page
			bcopy((char *)buffer, (char *)new_page, PAGE_SIZE);


			//Get address of code_table that new_page will be written to and mask out the entry bits
			uint32_t code_table = (current_running->page_directory[get_directory_index(current_running->fault_addr)]);
			code_table = code_table & PE_BASE_ADDR_MASK;

			//Insert new page into process_code_table, process_code_table is located at 0x103000
			insert_page(new_page, (uint32_t *)code_table, current_running->fault_addr, 0x7);
		}

		//If there is not possible to create new pages
		else if(next_free_memory > MAX_PHYSICAL_MEMORY-PAGE_SIZE){
			scrprintf(0,0, "No more free pages");

			/*
			-Pages from 0x100000 - 0x104000 contain kernel code that cannot be evicted (Kernel directory, kernel table, process directory, process code table, process stack table, process stack page)
			-Choose a page to evict from 0x105000 to 0x121000.
			*/
			uint32_t evict_page = (4096 * evict_count) + 0x105000;	//First time page 0x105000 will be evicted
			evict_count++;

			//Only have 28 pages to evict, so reset evict count if it exceeds 27 pages (from 0x120000 - 0x121000)
			if(evict_count > 27){
				evict_count = 1;
			}

			//Find address of process_code_table
			uint32_t *code_table = ((uint32_t *)current_running->page_directory[get_directory_index(current_running->fault_addr)]);
			scrprintf(4, 0, "fault address: %x", current_running->fault_addr);
			
			scrprintf(5, 0, "code table: %x", code_table);

			//Find address og page in code_table that will be erased
			uint32_t *erase_page = (uint32_t *)code_table[get_table_index(current_running->fault_addr)];
			
			//Find dirty bit for selected page
			uint32_t dirty_bit = (uint32_t)erase_page & PE_D;

			//If this page has been modified, write it back to disk first
			if(dirty_bit == 1){
				//Write contents of page into buffer
				char buffer[PAGE_SIZE];
				bcopy((char *)erase_page, (char *)buffer, PAGE_SIZE);
				
				//Find block in image-file where code was read from, so that it can be written back to same location
				uint32_t block_offset = erase_page[0];

				//Write contents into location in disk/image-file
				scsi_write(block_offset, PAGE_SIZE, (char *)buffer);

				//Erase all entries in chosen page
				for(uint32_t i=evict_page; i<evict_page+1024; i++){
					erase_page[i] = 0;
				}

				//Clean translation lookaside buffer
				flush_tlb_entry((uint32_t)erase_page);

				//Write missing code from disk to page

				//Find which block offset the virtual address corresponds to
				int offset = (current_running->fault_addr & PE_BASE_ADDR_MASK) - current_running->start_pc;
				offset = (offset / SECTOR_SIZE);
				
				int num_sectors = 0;

				//Find how many sectors remaining that should be read
				if(current_running->swap_size - offset >= 8){
					num_sectors = 8;
				}
				
				else if(current_running->swap_size - offset < 8){
					num_sectors = current_running->swap_size - offset;
				}

				//Read missing code from disk into buffer with offset of faulting address
				scsi_read(current_running->swap_loc+offset, num_sectors, (char *)buffer);

				//Copy buffer into page
				bcopy((char *)buffer, (char *)erase_page, PAGE_SIZE);
			}

			//If selected page has NOT been modified
			else if(dirty_bit == 0){
				//Erase all entries in chosen page
				for(uint32_t i=evict_page; i<evict_page+1024; i++){
					erase_page[i] = 0;
				}

				//Clean translation lookaside buffer
				flush_tlb_entry((uint32_t)erase_page);

				//Create buffer to write missing code into
				char buffer[PAGE_SIZE];
				
				//Find which block offset the virtual address corresponds to
				int offset = (current_running->fault_addr & PE_BASE_ADDR_MASK) - current_running->start_pc;
				offset = (offset / SECTOR_SIZE);
				
				int num_sectors = 0;

				//Find how many sectors remaining that should be read
				if(current_running->swap_size - offset >= 8){
					num_sectors = 8;
				}
				
				else if(current_running->swap_size - offset < 8){
					num_sectors = current_running->swap_size - offset;
				}

				//Read missing code from disk into buffer with offset of faulting address
				scsi_read(current_running->swap_loc+offset, num_sectors, (char *)buffer);

				//Mark all pages in buffer as present
				for(int i=0; i<1024; i++){
					buffer[i] = buffer[i] & PE_BASE_ADDR_MASK;
					buffer[i] = buffer[i] | 0x7;
				}

				//Copy buffer into new_page
				bcopy((char *)buffer, (char *)erase_page, PAGE_SIZE);
			}
		}
	
	lock_release(&paging_lock);
	}
}