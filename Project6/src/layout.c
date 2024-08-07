/* Hvordan tinger henger sammen:
-Disk superblock skal fylles ut og skrives på disk
-Mem superblock skal inneholde disk superblock pluss litt til. Skal ikke skrives på disk
-Et global Inode table med 18 mem_inode_t entries skal lages. 
	-Dette inode table skal fylles med mem_inde_t når de lages og må huskes på å bli skrevet til disk hver gang.
	-Dette inode tablet skal skrives til disk når det først blir laget og.

-Mem superblock bruker sin disk superblock til å finne hvilken block root inode ligger i. 
-Root inode ligger på første entry i denne data blocken
-Root inode er så en mem_inode_t som kan få tilgang til den samme inoden men som ligger på disk
-Inoden som ligger på disk har et array med pekere til datablocker
-Disse datablockene kan inneholde faktisk fildata eller de kan inneholde flere diren_t's. Det er plass til 25 dirent_t's i en data block?
	-Hvis det ligger en dirent_t i en data block, har denne et navn og en ny inode index i global inode table
	-Inode indexen i global inode table er så en mem_inode_t.
	-via Mem_inode_t får man tilgang til en identisk disk inode med pekere til nye datablocker. 
	-Disse datablockene kan så igjen inneholde fildata eller nye dirent_t's 
	-.....og så videre 


-En pcb inneholder en cwd variabel og en fd_entry struct
-pcb->cwd er en index i global inode table og er det directoriet som pcb'en er i for øyeblikket.
-Ved start så må pcb være i root directory, altså index 0 i global inode table
-Når man skifter directory (chdir) så må pcb->cwd endres til en ny index i global inode table.

-fd_entry som pcb'en inneholder er indexer til global inode table
-Hvis det ligger en index i pcb->fd_entry så er det en fil som pcb'en har åpnet. 
*/



/*Layout of file system on disk for p6sh

Block 0: Disk-Superblock:
	-number of inodes in system
	-number of data blocks in system
	-Block number for the root-inode: 1
	-Size of the largest file in the system
	-signature of superblock

Block 1. disk_inodes, first entry: Root inode
	-Root-inode
	    -type: INTYPE_DIR
		-size
		-nlinks
		-Array of pointers to data blocks[8]

Block 2. Bitmap containing Inode bitmap & data block bitmap
	-All 512 entries in bitmap is set to 0
*/


/*Layout of filesystem in memory for p6sh
-Global memory superblock
	-copy of disk superblock
	-Inode bitmap
	-Datablock bitmap
	-dirty

-Global file descriptor table. Array of mem_inode_t with [18 entries]
	-Only first entry is set to root-inode
	-Rest entries are empty until more files are made
*/

/*
Data block 0: Superblock
Data block 1: Inode table
Data block 2: Bitmap
Data block 3: datablock for root directory





*/
/*DISK SUPERBLOCK
-Ninodes = 1
-Ndatablock = 1
-Root_inode = 1
-Max_filesize
*/


/*INODE TABLE:
[0] mem_inode_t root_inode
[1]	 -1
[2]	 -1
[3]	 -1
[4]	 -1
[5]	 -1
[6]	 -1
[7]	 -1
[8]	 -1
[9]	 -1
[10] -1
[11] -1
[12] -1
[13] -1
[14] -1
[15] -1
[16] -1
[17] -1
*/

/*FILE DESCRIPTOR TABLE
[0]	fd_entry_t root_inode
[1]
[2]
[3]
[4]
[5]
[6]
[7]
[8]
[9]
[10]
*/

/*DATABLOCK CONTAINING DIRENTS
[0]	3
[1]	-1
[2]	-1
[3]	-1
[4]	-1
[5]	-1
[6]	-1
[7]	-1
[8]	-1
[9]	-1
[...]
[24] -1
*/


/*Stuff to remember:


*WHEN CREATING NEW INODE
	-Place new_inode in inode_table
	-Update inode_table to disk (write it to disk after entry is set)
	-Write new_inode->datablock to disk when created
	-Increment superblock->num_inodes

*WHEN CREATING NEW DATABLOCK:
	-Write datablock to disk
	-increment superblock->numdatablocks
	-Increment parent directory->size

*WHEN UPDATING DATABLOCK:
	-Update datablock to disk
	-increment superblock->numdatablocks
	-Increment parent directory->size



*QUESTIONS:
	-Hva skal superblock->max_filesize settes til?

	-Må jeg skrive bitmap til disk? Jeg bruker bare inode&dblk_bmap....




-if linkname exists
	-if filename does not exist
		-if there is space in datablock
		-set new inode
		-update inode_table
		-Update parent directory ->size
		-update linkname->datablock to disk


*/