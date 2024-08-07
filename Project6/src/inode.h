#ifndef INODE_H
#define INODE_H

/*
 * Index node as stored on disk. Holds everything about a file except
 * its name. The nlinks member is a count of the number of directories
 * that have this file as an entry. This can be used to
 * consistency-check the filesystem (the number of references made in all
 * directories should equal nlinks). The first NDIRECT blocks of the
 * file are located in the direct blocks. The rest are located in the
 * blocks listed in disk block given in indirect. The member type
 * describes the type of file this is (regular, directory). The size
 * member must be used to determine which direct and indirect entries
 * hold actual file data.
 */

#include "fstypes.h"

#define INODE_NDIRECT 8 /* number of direct disk blocks in an inode */

#define INTYPE_FILE 1
#define INTYPE_DIR 2

typedef struct disk_inode disk_inode_t;
struct disk_inode {
	short type;   /* file type */
	int size;     /* file size in bytes */
	short nlinks; /* number of directory entries referring to this file */
	/* pointers to the first NDIRECT blocks */
	blknum_t direct[INODE_NDIRECT];
	/*      blknum_t indirect; /\* The rest of the blocks *\/ */
};

#define INODE_BLK_SIZE 1

/*
 * Index node as used in memory; contains everything that is stored on
 * disk as well as:
 * open_count: The number of opens done on this file.
 * inode_num: its inode_number.
 * dirty: True if the inode needs to be updated on disk.
 * pos: The current read/write position in bytes from start (if we implement fork(), then
 * we can't have this field here anymore).
 */
typedef struct mem_inode mem_inode_t;
struct mem_inode {
	struct disk_inode d_inode;	//The similar inode reciding on disk
	short open_count;
	int pos;	//Used when reading from file
	int write_pos;	//Used when writing to file
	inode_t inode_num; 			//Index in inode table
	char dirty;
};

#endif /* INODE_H */
