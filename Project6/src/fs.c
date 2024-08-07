#include "fs.h"

#ifdef LINUX_SIM
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#endif /* LINUX_SIM */

#include "block.h"
#include "common.h"
#include "fs_error.h"
#include "inode.h"
#include "kernel.h"
#include "superblock.h"
#include "thread.h"
#include "util.h"

#define BITMAP_ENTRIES 256
#define INODE_TABLE_ENTRIES 20


//Allocate space for structures
mem_superblock_t superblock[SUPERBLK_SIZE];
int superblock_datablock;
mem_inode_t inode_table[BLOCK_SIZE/sizeof(disk_inode_t)];	//Inode table containing 18 entries. Is written to disk as well
char inode_bmap[BITMAP_ENTRIES];
char dblk_bmap[BITMAP_ENTRIES];
char bitmap[BITMAP_ENTRIES*2];	//Contains both inode and data block bitmap
fd_entry_t file_descriptor_table[MAX_OPEN_FILES];	//Table keeping track of open files

static int get_free_entry(unsigned char *bitmap);
static int free_bitmap_entry(int entry, unsigned char *bitmap);
static inode_t name2inode(char *name);
static blknum_t ino2blk(inode_t ino);
static blknum_t idx2blk(int index);

/*
 * Exported functions.
 */
void fs_init(void)
{
	//Initialize blocks
	block_init();

	//If the file system has not been initialized before
	if(superblock->d_super.signature != 1){
		// create new filesystem
		fs_mkfs();
	}
	//If file system has been created before, load it
	else{
		//Load existing file-system
	}
}

/*Creates a new file system.
 *Argument: kernel size*/
void fs_mkfs(void)
{
	// int os = get_free_entry((unsigned char*)dblk_bmap);
	// int kernel = get_free_entry((unsigned char*)dblk_bmap);

	////printf("\n..........FS_MKFS..........\n");
	//Get datablock entry for superblock
	superblock_datablock = get_free_entry((unsigned char*)dblk_bmap);
	////printf("superblock->datablock_num = %d\n", superblock_datablock);


	//Set disk superblock entries
	superblock->d_super.ninodes = 0;
	superblock->d_super.ndata_blks = 0;
	superblock->d_super.root_inode = get_free_entry((unsigned char*)dblk_bmap);
	//Initialize superblock signature for future loading of file system
	superblock->d_super.signature = 1;
	//printf("Inode_table datablock_num = %d\n", superblock->d_super.root_inode);

	superblock->d_super.max_filesize = BLOCK_SIZE * 8;
	
	//Set superblock entries
	superblock->ibmap = &inode_bmap;
	superblock->dbmap = &dblk_bmap;
	superblock->dirty = FALSE;

	//Write superblock->disk_superblock to disk 
	block_write(superblock_datablock, &superblock->d_super);

	//Initialize inode table
	for(int i=0; i<BLOCK_SIZE/sizeof(disk_inode_t); i++){
		inode_table[i].d_inode.type = 0; //Not specified file type yet
		inode_table[i].d_inode.size = 0;
		inode_table[i].d_inode.nlinks = 0;
		//Dont need to allocate data blocks yet

		//mem_inode variables
		inode_table[i].open_count = 0;
		inode_table[i].pos = 0;
		inode_table[i].write_pos = 0;
		inode_table[i].inode_num = -1; //Marked as free by negative number, since a new inode will have values from 0 to 18
		inode_table[i].dirty = FALSE;
	}


	//Write inode table to disk
	block_write(superblock->d_super.root_inode, &inode_table->d_inode);

	//Place inode bitmap&datablock bitmap in one block and write it do disk
	bcopy((const char*)inode_bmap, bitmap, 256);
	bcopy((const char*)dblk_bmap, bitmap, 256);
	int bitmap_pos = get_free_entry((unsigned char*)dblk_bmap);
	//printf("Bitmap->datablock_num = %d\n", bitmap_pos);

	block_write(bitmap_pos, &bitmap);


	//Create root inode
	mem_inode_t root_inode;
	root_inode.d_inode.type = INTYPE_DIR;
	root_inode.d_inode.size = (sizeof(dirent_t) * 2);
	root_inode.d_inode.nlinks = 0;
	root_inode.d_inode.direct[0] = get_free_entry((unsigned char*)dblk_bmap);	//Datablock number
	//printf("Root_inode->datablock_num = %d\n", root_inode.d_inode.direct[0]);


	root_inode.open_count = 0;
	root_inode.pos = 0;
	root_inode.write_pos = 0;
	root_inode.inode_num = get_free_entry((unsigned char*)inode_bmap);
	//printf("Root_inode->inode_num = %d\n", root_inode.inode_num);

	root_inode.dirty = FALSE;
	
	//Create array of dirents to insert in root_directory->datablock
	dirent_t root_dirents[DIRENTS_PER_BLK];

	//Set all entries in root directory->datablock to available
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		root_dirents[i].inode = -1;
		strcpy(root_dirents[i].name, "empty");
	}

	//Set parent and child directory in root_directory->datablock to itself
	strcpy(root_dirents[0].name, ".");
	strcpy(root_dirents[1].name, "..");
	root_dirents[0].inode = 0;
	root_dirents[1].inode = 0;

	
	//Write root_directory->datablock to disk
	block_write(root_inode.d_inode.direct[0], &root_dirents);

	//Place root_inode in inode_table
	inode_table[0] = root_inode;



	//Set file_descriptor_table entries to available
	for(int i=0; i<MAX_OPEN_FILES; i++){
		file_descriptor_table[i].idx = -1;
	}


	//Inode table and bitmap has been changed, so write them to disk again
	block_write(superblock->d_super.root_inode, &inode_table->d_inode);
	
	bcopy((const char*)inode_bmap, bitmap, 256);
	bcopy((const char*)dblk_bmap, bitmap, 256);
	block_write(bitmap_pos, &bitmap);
	//printf("Current_running->cwd = %d\n", current_running->cwd);
	//printf("..........FS_MKFS END..........\n\n");
}

/*Opens a directory file by placing current_running->inode number in file_descriptor_table
 *Returns the file_descriptor_table index where open directory file is placed*/
int fs_open(const char *filename, int mode)
{
	inode_t inode = name2inode(filename);

	//If "filename" exists
	if(inode >= 0){
		//Loop through file_descriptor_table
		for(int i=0; i<MAX_OPEN_FILES; i++){
			//If there is space in file_descriptor_table
			if(file_descriptor_table[i].idx == -1){
				//Place current_running->inode in file_descriptor_table
				// file_descriptor_table[i].idx = inode_table[current_running->cwd].inode_num;
				file_descriptor_table[i].idx = inode_table[inode].inode_num;

				file_descriptor_table[i].mode = mode;

				//Increment inode->open_count
				// inode_table[current_running->cwd].open_count++;
				inode_table[inode].open_count++;


				//Return file_descriptor_table index where file is placed
				return i;
			}
		}
		//printf("ERROR: Could not open file, no more space in file_descriptor_table\n");
		return FSE_NOMOREFDTE;
	}
	//If "filename" does NOT exist, create new file of type "FILE"
	else{
		//Check if there is space in inode_table for new file
		for(int i=0; i<BLOCK_SIZE/sizeof(disk_inode_t); i++){
			if(inode_table[i].inode_num == -1){
				//Check if there is space in file_descriptor_table for new file
				for(int j=0; j<MAX_OPEN_FILES; j++){
					if(file_descriptor_table[j].idx == -1){
						//Check if there is space in current_running->datablock for new entry
						dirent_t curr_run_datablock[DIRENTS_PER_BLK];
						block_read(inode_table[current_running->cwd].d_inode.direct[0], &curr_run_datablock);
						for(int k=0; k<DIRENTS_PER_BLK; k++){
							if(curr_run_datablock[k].inode == -1){
								//Create new inode
								mem_inode_t new_inode;
								new_inode.d_inode.type = INTYPE_FILE;
								new_inode.d_inode.size = 0;
								new_inode.d_inode.nlinks = 0;
								new_inode.d_inode.direct[0] = get_free_entry((unsigned char*)dblk_bmap);
								new_inode.open_count = 1;
								new_inode.pos = 0;
								new_inode.write_pos = 0;
								new_inode.inode_num = get_free_entry((unsigned char*)inode_bmap);
								new_inode.dirty = FALSE;

								//Place new_inode in inode_table
								inode_table[i] = new_inode;

								//Increment current_running->d_inode.size
								inode_table[current_running->cwd].d_inode.size += sizeof(dirent_t);
								
								//Update inode_table to disk
								block_write(superblock->d_super.root_inode, &inode_table->d_inode);

								//Place new_inode in current_running->datablock
								curr_run_datablock[k].inode = new_inode.inode_num;
								strcpy(curr_run_datablock[k].name, filename);


								//Update current_running->datablock to disk
								block_write(inode_table[current_running->cwd].d_inode.direct[0], &curr_run_datablock);

								//Place inode in file_descriptor_table
								file_descriptor_table[j].idx = new_inode.inode_num;
								file_descriptor_table[j].mode = mode;

								return j;

							}
							if(k == DIRENTS_PER_BLK){
								//printf("ERROR: No space in current_running->datablock for new entry\n");
								return FSE_ADDDIR;
							}
						}
					}
					if(j == MAX_OPEN_FILES){
						//printf("ERROR: No space in file_descriptor_table for new file\n");
						return FSE_NOMOREFDTE;
					}
				}
			}
			if(i == BLOCK_SIZE/sizeof(disk_inode_t)){
				//printf("ERROR: No more space in inode_table for new file\n");
				return FSE_NOMOREINODES;
			}
		}
	}
}

/*Removes an inode at index "fd" in file_descriptor_table*/
int fs_close(int fd)
{
	//Decrement inode->open_count at position "fd" in FD_table
	inode_t inode = file_descriptor_table[fd].idx;
	inode_table[inode].open_count--;
	
	//Remove inode from file_descriptor_table
	file_descriptor_table[fd].idx = -1;
	file_descriptor_table[fd].mode = MODE_UNUSED;

	return FSE_OK;	
}

/*Reads "size" number of bytes from position "inode.pos" from file "fd" into "buffer"*/
int fs_read(int fd, char *buffer, int size)
{
	// //printf("\n..........FS_READ..........\n");
	//Find out which inode we should read, from open files
	inode_t inode_num = file_descriptor_table[fd].idx;

	//Find out which type the inode is
	int inode_type = inode_table[inode_num].d_inode.type;

	//If inode is of type "DIRECTORY", read out dirents
	if(inode_type == INTYPE_DIR){
		//Read "size" number of bytes from inode->datablock into "buffer"
		while(inode_table[inode_num].pos <= inode_table[inode_num].d_inode.size - sizeof(dirent_t)){ //BLOCK_SIZE - sizeof(dirent_t)){
			block_read_part(inode_table[inode_num].d_inode.direct[0], inode_table[inode_num].pos, size, buffer);
			inode_table[inode_num].pos += size;

			return inode_table[inode_num].pos;
		}

		//Reached end of file, reset inode->pos and return
		inode_table[inode_num].pos = 0;
		return FSE_OK;
	}
	if(inode_type == INTYPE_FILE){
		//Read inode->datablock into memory
		while(inode_table[inode_num].pos <= BLOCK_SIZE){
			block_read_part(inode_table[inode_num].d_inode.direct[0], inode_table[inode_num].pos, size, buffer);
			inode_table[inode_num].pos += size;
			
			return inode_table[inode_num].pos;
		}

		//Reached end of file, reset inode->pos and return
		inode_table[inode_num].pos = 0;
		return FSE_OK;
	}

}

/*Write "buffer" into "fd"->datablock*/
int fs_write(int fd, char *buffer, int size)
{
	//Find out which inode we should write to, from open files
	inode_t inode = file_descriptor_table[fd].idx;

	//Check if inode is of type "FILE"
	if(inode_table[inode].d_inode.type == INTYPE_FILE){
		if(inode_table[inode].d_inode.size <= (512*8) - size){

			block_modify(inode_table[inode].d_inode.direct[0], inode_table[inode].write_pos, buffer, size);
			inode_table[inode].write_pos += size;

			//Increase inode->size
			inode_table[inode].d_inode.size += size;

			//Update inode_table to disk
			block_write(superblock->d_super.root_inode, &inode_table->d_inode);
			return FSE_OK;
		}
		else{
			//printf("ERROR: No more space in file to write to\n");
			return FSE_FULL;
		}
	}
	//If inode is of type "DIRECTORY", return error
	else{
		//printf("ERROR: Can not write to file of type: DIRECTORY\n");
		return FSE_INVALIDMODE;
	}
}

/*This function is really incorrectly named, since neither its offset
 *argument or its return value are longs (or off_t's)*/
int fs_lseek(int fd, int offset, int whence)
{
	//Find out which inode we should write to, from open files
	inode_t inode = file_descriptor_table[fd].idx;
	
	switch(whence){
		case SEEK_SET:
			inode_table[inode].pos = 0+offset;
			break;
	
		case SEEK_CUR:
			inode_table[inode].pos += offset;
			break;
		
		case SEEK_END:
			inode_table[inode].pos = superblock->d_super.max_filesize + offset;
			break;
	}
	
	return FSE_OK;
}

/*Create new directory*/
int fs_mkdir(char *dirname)
{
	//Create new inode
	mem_inode_t new_inode;
	new_inode.d_inode.type = INTYPE_DIR;
	new_inode.d_inode.size = (sizeof(dirent_t) * 2);
	new_inode.d_inode.nlinks = 0;
	new_inode.d_inode.direct[0] = get_free_entry((unsigned char*)dblk_bmap);
	new_inode.open_count = 0;
	new_inode.pos = 0;
	new_inode.write_pos = 0;
	new_inode.inode_num = get_free_entry((unsigned char*)inode_bmap);
	new_inode.dirty = FALSE;

	//Loop through inode_table
	for(int i=0; i<BLOCK_SIZE/sizeof(disk_inode_t); i++){
		//If space in inode_table, place inode		
		if(inode_table[i].inode_num == -1){
			inode_table[i] = new_inode;
			break;
		}
		//No space in inode_table, return error
		if(i == BLOCK_SIZE/sizeof(disk_inode_t)){
			//printf("ERROR: inode_table full\n");
			return FSE_INODETABLEFULL;
		}
	}


	//Update inode_table to disk
	block_write(superblock->d_super.root_inode, &inode_table->d_inode);



	//Create array of dirents to put in new_inode->datablock
	dirent_t new_dirents[DIRENTS_PER_BLK];

	//Set all entries in dirents to available
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		new_dirents[i].inode = -1;
		strcpy(new_dirents[i].name, "empty");
	}

	//Make dirents[0] point to root directory and dirents[1] point to current_running_cwd (parent directory)
	strcpy(new_dirents[0].name, ".");
	strcpy(new_dirents[1].name, "..");
	new_dirents[0].inode = new_inode.inode_num;		//Points to itself 
	new_dirents[1].inode = current_running->cwd;	//Point to parent directory

	//Write dirents to new_inode->datablock
	block_write(new_inode.d_inode.direct[0], &new_dirents);





	//Increment number of inodes in superblock
	superblock->d_super.ninodes++;
	superblock->d_super.ndata_blks++;
	//Update superblock to disk
	block_write(superblock_datablock, &superblock->d_super);



	//Write current_running->datablock into memory
	dirent_t curr_run_dirents[DIRENTS_PER_BLK];
	int pos = 0;
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		block_read_part(inode_table[current_running->cwd].d_inode.direct[0], pos, sizeof(dirent_t), &curr_run_dirents[i]);
		pos += sizeof(dirent_t);
	}
	
	//Check if current_running->datablock already has an identical filename
	for(int i=0; i<DIRENTS_PER_BLK; i++){
	 	if(same_string(curr_run_dirents[i].name, dirname) == 1){
			//printf("ERROR: file already exists\n");
			return FSE_EXIST;
	 	}
	}
	



	//IDENTICAL FILENAME DOES NOT EXIST
	
	//Loop through current_running->datablock
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		//If available space in datablock
	 	if(curr_run_dirents[i].inode == -1){
			//Place new_directory
			curr_run_dirents[i].inode = new_inode.inode_num;
			strcpy(curr_run_dirents[i].name, dirname);

			//Write curr_run_dirents back to current_running->datablock
			block_write(inode_table[current_running->cwd].d_inode.direct[0], &curr_run_dirents);

			//Increment current_running->d_inode.size
			inode_table[current_running->cwd].d_inode.size += sizeof(dirent_t);

			//Update inode_table to disk
			block_write(superblock->d_super.root_inode, &inode_table->d_inode);

			return FSE_OK;
	 	}
	}

	//Could not write new_directory to current_running->datablock
	//printf("ERROR: could not add directory entry\n");
	
	return FSE_ADDDIR;
}

/*Changes current_running->cwd to another inode*/
int fs_chdir(char *path)
{	
	//Read current_running->datablock into memory
	dirent_t curr_run_dirents[DIRENTS_PER_BLK];
	int pos = 0;
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		block_read_part(inode_table[current_running->cwd].d_inode.direct[0], pos, sizeof(dirent_t), &curr_run_dirents[i]);
		
		//Check if a dirent in current_running->dirents matches "path"
		if(same_string(curr_run_dirents[i].name, path) == 1){
			//Check if dirent is of type "DIRECTORY"
			if(inode_table[curr_run_dirents[i].inode].d_inode.type == INTYPE_DIR){
				current_running->cwd = curr_run_dirents[i].inode;
				return FSE_OK;
			}
			else{
				//printf("ERROR: Can not change directory to an inode of type 'FILE'\n");
				return FSE_DIRISFILE;
			}
		}

		pos += sizeof(dirent_t);
	}

	//COULD NOT CHANGE PATH
	//printf("ERROR: could not change directory\n");
	return FSE_INVALIDNAME;
}

/*Removes a directory entry from current_running->datablocks*/
int fs_rmdir(char *path)
{
	//If user tries to remove directory entry "." or ".."
	if((same_string(path, ".") == 1) | (same_string(path, "..") == 1)){
		//printf("ERROR: Can not remove '.' and '..' directories\n");
		return FSE_ERROR;
	}

	//Copy current_running->datablock into memory
	dirent_t curr_run_datablock[DIRENTS_PER_BLK];
	int pos = 0;
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		block_read_part(inode_table[current_running->cwd].d_inode.direct[0], pos, sizeof(dirent_t), &curr_run_datablock[i]);
		pos += sizeof(dirent_t);
	}

	
	//Loop through current_running->datablock
	for(int i=0; i<DIRENTS_PER_BLK; i++){
		//If matching filename is found in datablock
		if(same_string(curr_run_datablock[i].name, path) == 1){
			//if chosen directory is NOT empty, return error
			if(inode_table[curr_run_datablock[i].inode].d_inode.size > sizeof(dirent_t) * 2){
				//printf("ERROR: Can not remove directory that is not empty\n");
				return FSE_DNOTEMPTY;
			}

			//If there are any links to the chosen directory
			if(inode_table[curr_run_datablock[i].inode].d_inode.nlinks > 0){
				//printf("ERROR: Can not remove directory. It has links to it\n");
				return FSE_ERROR;
			}

			//If chosen directory is open
			if(inode_table[curr_run_datablock[i].inode].open_count > 0){
				//printf("ERROR: Can not remove directory that is open\n");
				return FSE_FILEOPEN;
			}

			//If chosen inode is not of type directory, return error
			if(inode_table[curr_run_datablock[i].inode].d_inode.type != INTYPE_DIR){
				//printf("ERROR: Can not remove directory. It is of type: file\n");
				return FSE_DIRISFILE;
			}

			//PASSED ALL CHECKS, REMOVE DIRECTORY

			//Free datablock used by the directory_entry->inode
			free_bitmap_entry(inode_table[curr_run_datablock[i].inode].d_inode.direct[0], (unsigned char*)dblk_bmap);
			free_bitmap_entry(inode_table[curr_run_datablock[i].inode].inode_num, (unsigned char*)inode_bmap);

			//Remove directory entry from datablock
			curr_run_datablock[i].inode = -1;
			strcpy(curr_run_datablock[i].name, "empty");


			//Decrease size of parent directory
			inode_table[curr_run_datablock[i].inode].d_inode.size -= sizeof(dirent_t);
			if(inode_table[curr_run_datablock[i].inode].d_inode.size < 0){
				inode_table[curr_run_datablock[i].inode].d_inode.size = 0;
			}


			//Write current_running->datablock back to disk
			block_write(inode_table[current_running->cwd].d_inode.direct[0], &curr_run_datablock);

			return FSE_OK;
		}
	}

	//printf("ERROR: Can not remove directory that does no exist\n");
	return FSE_NOTEXIST;
}

/*Makes a copy with name "filename" and set its inode to the same inode as "linkname"*/
int fs_link(char *linkname, char *filename)
{
	int linkname_inode = name2inode(linkname);
	int filename_inode = name2inode(filename);

	//If file "linkname" exists
	if(linkname_inode >= 0){
		//If "filename" does NOT exist
		if(filename_inode == -1){
			
			dirent_t dirent;
			int pos = 0;
			for(int i=0; i<DIRENTS_PER_BLK; i++){
				block_read_part(inode_table[current_running->cwd].d_inode.direct[0], pos, sizeof(dirent_t), &dirent);

				//If there is space in datablock for new entry
				if(dirent.inode == -1){
					//Set new filename but same inode
					strcpy(dirent.name, filename);
					dirent.inode = linkname_inode;

					//Update "linkname"->link_count
					inode_table[linkname_inode].d_inode.nlinks++;

					//Update inode_table to disk
					block_write(superblock->d_super.root_inode, &inode_table->d_inode);

					//Update parent_directory->size
					inode_table[current_running->cwd].d_inode.size += (sizeof(dirent_t));

					//Update linkname->datablock to disk
					block_modify(inode_table[current_running->cwd].d_inode.direct[0], pos, &dirent, sizeof(dirent_t));
		
					return FSE_OK;
				}
			
				pos += sizeof(dirent_t);
			}
		}
		else{
			//printf("ERROR: Can not link a file to another file that already exists.\n");
			return FSE_EXIST;
		}
	}
	else{
		//printf("ERROR: Can not link to a file that does not exist.\n");
		return FSE_NOTEXIST;
	}
}

/*Removes a link or deletes a file if linkcount == 0*/
int fs_unlink(char *linkname) {
	//printf("\n..........FS_UNLINK..........\n");

	//If user tries to remove/unlink directory entry "." or ".."
	if((same_string(linkname, ".") == 1) | (same_string(linkname, "..") == 1)){
		//printf("ERROR: Can not remove '.' and '..' directories\n");
		return FSE_ERROR;
	}
	
	//Check if "linkname" exists
	inode_t inode = name2inode(linkname);
	//printf("	Name2inode found inode num: %d\n", inode);

	if(inode >= 0){
			//Loop through current_running->datablock
		dirent_t dirent;
		int pos = 0;
		for(int i=0; i<DIRENTS_PER_BLK; i++){
			block_read_part(inode_table[current_running->cwd].d_inode.direct[0], pos, sizeof(dirent_t), &dirent);

			//Delete directory entry and inode if linkname is found
			if(same_string(dirent.name, linkname) == 1){
				//printf("Found matching dirent\n");
				strcpy(dirent.name, "empty");
				dirent.inode = -1;

				//Modify current_running->datablock on disk
				block_modify(inode_table[current_running->cwd].d_inode.direct[0], pos, &dirent, sizeof(dirent_t));

				//If inode->nlinks == 0, delete inode and datablock for linkname
				if(inode_table[inode].d_inode.nlinks == 0){
					//printf("Links to inode is ZERO\n");

					//Delete linkname->inode and datablock
					free_bitmap_entry(inode_table[inode].d_inode.direct[0], (unsigned char*)dblk_bmap);
					free_bitmap_entry(inode_table[inode].inode_num, (unsigned char*)inode_bmap);

					return FSE_OK;
				}
				//If linkname has some links to it, decrement linkname->nlinks
				else{
					inode_table[inode].d_inode.nlinks--;

					//Update inode_table to disk
					block_write(superblock->d_super.root_inode, &inode_table->d_inode);

					return FSE_OK;
				}
			}

			pos += sizeof(dirent_t);
		}
		//If no entries in current_running->datablock matches "linkname"
		//printf("ERROR: Did not find 'linkname' in current_running->datablock.\n");
		return FSE_DENOTFOUND;	
	}
	//Linkname does not exist
	else{
		//printf("ERROR: unlink name does not exist.\n");
		return FSE_DENOTFOUND;
	}
}

/*Prints a files: filename, type, nlinks and size*/
int fs_stat(int fd, char *buffer)
{
	//Find inode number at position "fd" in file_descriptor_table
	inode_t inode = file_descriptor_table[fd].idx;

	//Load contents of inode into buffer
	buffer[0] = inode_table[inode].d_inode.type;
	buffer[1] = inode_table[inode].d_inode.nlinks;
	int size = inode_table[inode].d_inode.size;
	bcopy(&size, &buffer[2], sizeof(int));

	return FSE_OK;
}

/*
 * Helper functions for the system calls
 */

/* Search the given bitmap for the first zero bit.  If an entry is
 * found it is set to one and the entry number is returned.  Returns
 * -1 if all entrys in the bitmap are set.
*/
static int get_free_entry(unsigned char *bitmap) {
	int i;

	/* Seach for a free entry */
	for (i = 0; i < BITMAP_ENTRIES / 8; i++) {
		if (bitmap[i] == 0xff) /* All taken */
			continue;
		if ((bitmap[i] & 0x80) == 0) { /* msb */
			bitmap[i] |= 0x80;
			return i * 8;
		}
		else if ((bitmap[i] & 0x40) == 0) {
			bitmap[i] |= 0x40;
			return i * 8 + 1;
		}
		else if ((bitmap[i] & 0x20) == 0) {
			bitmap[i] |= 0x20;
			return i * 8 + 2;
		}
		else if ((bitmap[i] & 0x10) == 0) {
			bitmap[i] |= 0x10;
			return i * 8 + 3;
		}
		else if ((bitmap[i] & 0x08) == 0) {
			bitmap[i] |= 0x08;
			return i * 8 + 4;
		}
		else if ((bitmap[i] & 0x04) == 0) {
			bitmap[i] |= 0x04;
			return i * 8 + 5;
		}
		else if ((bitmap[i] & 0x02) == 0) {
			bitmap[i] |= 0x02;
			return i * 8 + 6;
		}
		else if ((bitmap[i] & 0x01) == 0) { /* lsb */
			bitmap[i] |= 0x01;
			return i * 8 + 7;
		}
	}
	return -1;
}

/* Free a bitmap entry, if the entry is not found -1 is returned, otherwise zero.
 * Note that this function does not check if the bitmap entry was used (freeing
 * an unused entry has no effect).
 */
static int free_bitmap_entry(int entry, unsigned char *bitmap) {
	unsigned char *bme;

	if (entry >= BITMAP_ENTRIES)
		return -1;

	bme = &bitmap[entry / 8];

	switch (entry % 8) {
	case 0:
		*bme &= ~0x80;
		break;
	case 1:
		*bme &= ~0x40;
		break;
	case 2:
		*bme &= ~0x20;
		break;
	case 3:
		*bme &= ~0x10;
		break;
	case 4:
		*bme &= ~0x08;
		break;
	case 5:
		*bme &= ~0x04;
		break;
	case 6:
		*bme &= ~0x02;
		break;
	case 7:
		*bme &= ~0x01;
		break;
	}

	return 0;
}

/* Returns the filesystem block (block number relative to the super
 * block) corresponding to the inode number passed.*/
static blknum_t ino2blk(inode_t ino) {
	return (blknum_t)-1;
}

/* Returns the filesystem block (block number relative to the super
 * block) corresponding to the data block index passed.*/
static blknum_t idx2blk(int index) {
	return (blknum_t)-1;
}

/* Parses a file name and returns the corresponding inode number. If
 * the file cannot be found, -1 is returned.
 * Loops through inode table to see if names from path exists
 */
static inode_t name2inode(char *name) 
{		
	//If "name" == "/", return 0
	if(same_string(name, "/") == 1){
		return 0;
	}


	int current_inode = -1;
	int current_read_pos = 0;

	while(name[current_read_pos] != '\0'){
		if(name[current_read_pos] == '/'){
			current_read_pos++;
		}
		else{
			int i = 0;
			char path_name[MAX_FILENAME_LEN];

			while(name[current_read_pos] != '\0' && name[current_read_pos] != '/'){
				//Last første dir navn inn i array
				path_name[i] = name[current_read_pos];
				
				i++;				
				current_read_pos++;
			}

			//Place a '\0' terminator at end of path_name
			path_name[i] = '\0';
			//There was a bug with path_name, but copying it to "test" worked....
			char test[MAX_FILENAME_LEN];
			strcpy(&test, &path_name);


			//Loop through entries in inode_table
			for(int j=0; j<BLOCK_SIZE/sizeof(disk_inode_t); j++){
				int found_inode = 0;
				//If there is an inode in inode_table
				if(inode_table[j].inode_num != -1){
					//Read inode->datablock into memory
					dirent_t dirents[DIRENTS_PER_BLK];
					block_read(inode_table[j].d_inode.direct[0], &dirents);

					//Check if entry in inode->datablock matches path_name
					for(int k=0; k<DIRENTS_PER_BLK; k++){
						if(same_string(dirents[k].name, test) == 1){
							current_inode = dirents[k].inode;
							found_inode = 1;
							break;
						}
					}
				}
				if(found_inode == 1){
					break;
				}
			}

		}
	}

	if(current_inode < 0){
		return -1;
	}
	else{
		return current_inode;
	}
}

/*Parses a given path.
 *Counts how many directories are in that path, including root directory
 *Returns number of directories in path*/
int parse_num(char *path)
{	
	//Find how many characters path consists of
	int path_size = strlen(path);

	//If path only consists of 1 character, it means we are in root
	if(path_size == 1){
		return 1;
	}

	//If path is not only in root
	else{
		int size = 1;	//Size is returned. Start at 1 to include root_directory
		//Loop through "path" and increase size if "/" is found
		for(int i=0; i<path_size; i++){
			if(path[i] == '/'){
				size++;
			}	
		}
		//Return how many directories are found
		return size;
	}
}

/*Prints inode_table*/
void print_inode_table()
{
	for(int i=0; i<BLOCK_SIZE/sizeof(disk_inode_t); i++){
		//printf("inode_table[%d].inode = %d\n", i, inode_table[i].inode_num);
	}
}

/*Prints file_descriptor_table*/
void print_FD_table()
{
	for(int i=0; i<MAX_OPEN_FILES; i++){
		//printf("FD_table[%d].inode = %d\n", i, file_descriptor_table[i].idx);
	}
}

/*Prints all directory entries in datablock*/
void print_dirents(blknum_t block_number)
{
	//printf("Printing dirents from datablock[%d]\n", block_number);
	dirent_t dirent;
	int pos = 0;

	for(int i=0; i<DIRENTS_PER_BLK; i++){
		block_read_part(block_number, pos, sizeof(dirent_t), &dirent);
		
		//printf("dirent[%d]: inode[%d], name[%s]\n", i, dirent.inode, dirent.name);
		
		pos += sizeof(dirent_t);
	}
}

/*Prints char contents from given datablock*/
void print_characters(blknum_t block_number)
{
	//printf("Printing characters from datablock[%d]\n", block_number);
	char characters[BLOCK_SIZE];
	int pos = 0;

	block_read(block_number, &characters);

	for(int i=0; i<BLOCK_SIZE; i++){
		//printf("Datablock[%d] = %c\n", i, characters[i]);
	}
}