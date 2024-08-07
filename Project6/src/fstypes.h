#ifndef FSTYPES_H
#define FSTYPES_H

typedef short blknum_t; /* type for disk block number */

typedef int inode_t; /* type for index node number */

/* filedescriptor entry */
typedef struct fd_entry fd_entry_t;
struct fd_entry {
	int idx;           //USED TO KEEP INODE NUMBER /* index into the global inode_table */ 
	unsigned int mode; /* mode bits (see MODE_XXX enum vals in fs.h) */
};

/* per-process maximum open file count */
#define MAX_OPEN_FILES 10

#endif /* FSTYPES_H */