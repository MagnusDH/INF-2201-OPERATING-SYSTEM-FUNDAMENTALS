#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define IMAGE_FILE "image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

/* Variable to store pointer to program name */
char *progname;

/* Variable to store pointer to the filename for the file being read. */
char *elfname;

/* Structure to store command line options */
static struct {
	int vm;
	int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);

int main(int argc, char **argv) {
	/* Process command line options */
	progname = argv[0];
	options.vm = 0;
	options.extended = 0;
	while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
		char *option = &argv[1][2];

		if (strcmp(option, "vm") == 0) {
			options.vm = 1;
		}
		else if (strcmp(option, "extended") == 0) {
			options.extended = 1;
		}
		else {
			error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
		}
		argc--;
		argv++;
	}
	if (options.vm == 1) {
		/* This option is not needed in project 1 so we doesn't bother
		 * implementing it*/
		error("%s: option --vm not implemented\n", progname);
	}
	if (argc < 3) {
		/* at least 3 args (createimage bootblock kernel) */
		error("usage: %s %s\n", progname, ARGS);
	}
	create_image(argc - 1, argv + 1);
	return 0;
}

void parse_bootblock(char *bootblock_file)
{
	//Defining Elf32 structures to help getting important variables
	Elf32_Ehdr ehdr;											//Elf header
	Elf32_Phdr phdr;											//Elf program header

	//Opening given bootblock file in "read byte" mode
	FILE *file = fopen(bootblock_file, "r+b");
	if(file == NULL){
		error("Could not open bootblock file...Exiting\n");
	}

	//Locating program-header_table via elf-header
	int ret1 = fread(&ehdr, sizeof(Elf32_Ehdr), 1, file);		//Reading entire "Elf header" into &ehdr
	fseek(file, ehdr.e_phoff, SEEK_SET);						//Moving cursor from beginning of bootblock file to the program_header offset
	int ret2 = fread(&phdr, sizeof(Elf32_Phdr), 1, file);		//Reading entire "elf program_header" into &phdr
	fseek(file, phdr.p_offset, SEEK_SET);						//Moving cursor to where program_offset starts (the executable code)

	//Allocating 512 bytes for executable code section, to be able to modify it
	char *exec_code = calloc(512, sizeof(char));
	if(exec_code == NULL){
		error("Could not allocate space for executable code in parse_bootblock function\n");
	}

	//Reading executable code into the newly created array
	int ret3 = fread(exec_code, phdr.p_filesz, 1, file); 		//The cursor is currently standing at the correct point to be read from

	//Modifying bytes in array to magic numbers and kernelSize
	exec_code[510] = 0x55;										//Magic number
	exec_code[511] = 0xaa;										//Magic number
	exec_code[2] = 9;											//kernelSize (location 2 is taken from powerpoint presentation and kernelsize is found in kernel-file (phdr.p_filesz/512bytes))


	//Creating and opening new file to write code to. This file will be the IMAGE file
	FILE *image = fopen(IMAGE_FILE, "a+b");						//Opening image-file in "append byte" mode
	fwrite(exec_code, 1, 512, image);							//Writing code to image-file

	//Closing opened bootblock_file and IMAGE_FILE
	fclose(file);
	fclose(image);

	//Freeing up space taken for "exec_code"
	free(exec_code);

	//Checking return values for fread()
	if(ret1 == 0){
		error("Could not copy elf_header from bootblock file...Exiting\n");
	}
	if(ret2 == 0){
		error("Could not copy 'program_header' from bootblock file...Exiting\n");
	}
	if(ret3 == 0){
		error("Could not copy 'program_offset' from bootblock file...Exiting\n");
	}

	return;
}

void parse_kernel(char *kernel_file)
{
	//Defining Elf32 structures to help getting important variables
	Elf32_Ehdr ehdr;	//Elf header
	Elf32_Phdr phdr;	//Elf program header

	//Opening given kernel file in "read byte" mode
	FILE *file = fopen(kernel_file, "r+b");
	if(file == NULL){
		error("Could not open kernel file...Exiting\n");
	}

	//Locating program-header_table via elf-header
	int ret1 = fread(&ehdr, sizeof(Elf32_Ehdr), 1, file);		//Reading entire "Elf header" into &ehdr
	fseek(file, ehdr.e_phoff, SEEK_SET);						//Moving cursor from beginning of kernel file to the program_header offset
	int ret2 = fread(&phdr, sizeof(Elf32_Phdr), 1, file);		//Reading entire "elf program_header" into &phdr
	fseek(file, phdr.p_offset, SEEK_SET);						//Moving cursor to where program_offset starts (the executable code)

	//Finding size of kernel in sectors
	float kernel_size = (float)phdr.p_filesz;					//Size of kernel program
	kernel_size = kernel_size/512;								//Fetching numbers of sectors
	kernel_size = ceil(kernel_size);							//Round sector number up

	//Allocating number of kernel_sectors * 512 bytes for executable code section, to be able to modify it
	char *exec_code = calloc((kernel_size*512), sizeof(char));
	if(exec_code == NULL){
		error("Could not allocate space for executable code in parse_kernel function...Exiting\n");
	}

	//Reading executable code into the newly created array
	int ret3 = fread(exec_code, phdr.p_filesz, 1, file); //The cursor is currently standing at the correct point to be read from

	//Opening image-file to "append" code to
	FILE *image = fopen(IMAGE_FILE, "a+b");

	//Writing code from array to IMAGE_FILE in second segment
	fwrite(exec_code, 512, kernel_size, image);							//Writing code to image-file

	//Closing opened kernel_file
	fclose(file);
	fclose(image);

	//Freeing up space taken for "exec_code"
	free(exec_code);

	//Checking return values for fread()
	if(ret1 == 0){
		error("Could not copy elf_header from kernel file...Exiting\n");
	}
	if(ret2 == 0){
		error("Could not copy 'program_header' from kernel file...Exiting\n");
	}
	if(ret3 == 0){
		error("Could not copy 'program_offset' from kernel file...Exiting\n");
	}

	return;
}

void parse_additionalfile(char *additional_file)
{
	//Defining Elf32 structures to help getting important variables
	Elf32_Ehdr ehdr;	//Elf header
	Elf32_Phdr phdr;	//Elf program header

	//Opening given file in "read byte" mode
	FILE *file = fopen(additional_file, "r+b");
	if(file == NULL){
		error("Could not open additional file...Exiting\n");
	}

	//Locating program-header_table via elf-header
	int ret1 = fread(&ehdr, sizeof(Elf32_Ehdr), 1, file);		//Reading entire "Elf header" into &ehdr
	fseek(file, ehdr.e_phoff, SEEK_SET);						//Moving cursor from beginning of file to the program_header offset
	int ret2 = fread(&phdr, sizeof(Elf32_Phdr), 1, file);		//Reading entire "elf program_header" into &phdr
	fseek(file, phdr.p_offset, SEEK_SET);						//Moving cursor to where program_offset starts (the executable code)

	//Finding size of executable program in sectors
	float program_size = (float)phdr.p_filesz;					//Size of executable program
	program_size = program_size/512;							//Fetching numbers of sectors
	program_size = ceil(program_size);							//Round sector number up

	//Allocating number of program_sectors * 512 bytes for executable code section
	char *exec_code = calloc((program_size*512), sizeof(char));	//Calloc is used to padd files correctly. Space not used are written with zero's
	if(exec_code == NULL){
		error("Could not allocate space for executable code in parse_additionalfile function...Exiting\n");
	}

	//Reading executable code into the newly created array
	int ret3 = fread(exec_code, phdr.p_filesz, 1, file); //The cursor is currently standing at the correct point to be read from

	//Opening image-file to "append" code to
	FILE *image = fopen(IMAGE_FILE, "a+b");

	//Writing code from array to IMAGE_FILE in second segment
	fwrite(exec_code, 512, program_size, image);							//Writing code to image-file

	//Closing opened file
	fclose(file);
	fclose(image);

	//Freeing up space taken for "exec_code"
	free(exec_code);

	//Checking return values for fread()
	if(ret1 == 0){
		error("Could not copy elf_header from additional file...Exiting\n");
	}
	if(ret2 == 0){
		error("Could not copy 'program_header' from additional file...Exiting\n");
	}
	if(ret3 == 0){
		error("Could not copy 'program_offset' from additional file...Exiting\n");
	}

	return;
}

static void create_image(int nfiles, char *files[]) {
	printf("\nRunning program...\n\n");
	printf("%d files are given as arguments\n", nfiles);

	//Parsing bootblock file
	printf("Parsing bootblock\n");
	parse_bootblock(files[0]);

	//Parsing kernel file
	printf("Parsing kernel\n");
	parse_kernel(files[1]);

	//Parsing any additional files
	if(nfiles > 2){
		for(int i = 2; i <= nfiles; i++){
			printf("Parsing additional file nr.%d\n", i-1);
			parse_additionalfile(files[i]);
		}
	}
}

/* print an error message and exit */
static void error(char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	if (errno != 0) {
		perror(NULL);
	}
	exit(EXIT_FAILURE);
}