#  File System

#To run the simulator you will need to download the Bochs emulator or use the given shell simulator.

#The code can be compiled and run using the Bochs emulator by typing to following commands in terminal
	1. make clean
	2. make
	3. bochs -q

# Or it can be run in the shell simulator by typing the following commands:
	1. make clean
	2. make p6sh
	3. ./p6sh

#To use the file system simulator the following commands can be made:
	1. "ls" 		-prints the contents of the current directory
	2. "cat filename" 	-Write to an existing filename or create a new file
	3. "." 		-will end writing to a file
	4. "more filename"	-Print character contents of a file
	5. "mkdir filename"	-create a new directory
	6. "rmdir filename" 	-Delete a directory
	7. "cd filename"	-Change directory
	8. "ln linkname filename"-Creates a copy of linkname called filename
	9. "rm filename"	-Remove a link or delete a file
	10. "stat filename"	-Print details of a file
