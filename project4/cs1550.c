/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

/*PROTOTYPES*/
static void init_bitmap(void);
static void set_bit(int bit_index);
static int get_bit(int bit_index);
static int find_free_bit(void);

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

int bitmap[320]; //10240 divide by 32
int bitmap_loaded = 0;
/******************************************************************************
 *
 *  DUE DECEMBER 09, 2016
 *	cs1550_getattr, cs1550_mkdir, cs1550_readdir
 *****************************************************************************/

 /*HELPER FUNCTIONS*/
 static void init_bitmap(){

	 //bitmap[bitmap_index/32] |= (1 << (bitmap_index % 8))
	 memset(bitmap, 0, sizeof(bitmap));
	 bitmap[0/32] |= (1 << (0 % 32)); //root
	 bitmap[10237/32] |= (1 << (10237% 32));
	 bitmap[10238/32] |= (1 << (10238 % 32));
	 bitmap[10239/32] |= (1 << (10239 % 32));
	 bitmap_loaded = 1;
	 //seek -3*diskblock
	 FILE *disk = fopen(".disk", "r+b");
	 fseek(disk, -3*sizeof(cs1550_disk_block), SEEK_END);
	 fwrite(bitmap, sizeof(bitmap), 1, disk);

	 rewind(disk);
	 cs1550_root_directory *root;
	 root = calloc(1, sizeof(cs1550_root_directory));
	 fwrite((void*)&root, sizeof(cs1550_root_directory), 1, disk);
	 printf("Bitamp init = true\n");
	 fclose(disk);


 }
 static void set_bit(int bit_index){
	 bitmap[bit_index/32] |= (1 << (bit_index % 32));
 }
 static int get_bit(int bit_index){
	 return  bitmap[bit_index/32] &= (1 << (bit_index % 32));
 }
 static int find_free_bit(){
	 int i = 0;
	 while(i < 10240){
		 int j = (bitmap[j/32] &= (1 << (10238% 32)));
		 if (j == 0){
			 return i;
		 }
		 i++;
	 }
	 return -1;
 }

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
 /*
 DESCRIPTION
	 This function should look up the input path to determine
	 if it is a directory or a file.
	 If it is a directory, return the appropriate permissions.
	 If it is a file, return the appropriate permissions
	 as well as the actual size.
	 This size must be accurate since it is used to
	 determine EOF and thus read may not be called.
 UNIX
 	man -s 2 stat
 RETURN VALUES
	 0 on success with a correctly set structure
	-ENOENT if the file is not found
 */
 //DUE DECEMBER 4
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	printf("start getattr\n");
	if(!bitmap_loaded){
		init_bitmap();
	}
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//strings for directory name, filename, and file extension
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);


	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else { //strcmp(path, "/") == 1
		cs1550_directory_entry *dir;
		cs1550_root_directory *root;
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		//get root
		//cs1550_root_directory root;
		root = calloc(1, sizeof(cs1550_root_directory));
		FILE *file = fopen(".disk", "rb");
		fread(root, sizeof(cs1550_root_directory), 1, file);
		fclose(file);

		//Check if name is subdirectory
		/*
			//Might want to return a structure with these fields
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			res = 0; //no error
		*/
			//Might want to return a structure with these fields
		//check directories
		if(strcmp(filename, "") == 0){
			int i;
			int directory_location = -1;
			//root_directory.directories = root -> directories;
			for(i = 0; i < root->nDirectories; i++){
				if (strcmp(directory, root->directories[i].dname) == 0) {
					//sub directory exisits
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					res = 0; //no error
					directory_location = i;
				}
			}
			if(directory_location == -1){
				printf("Not Existing dir\n");
				//res = -ENOENT;
				res = 0;
			}
		}
		//Check if name is a regular file
		/*
			//regular file, probably want to be read and write
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 1; //file links
			stbuf->st_size = 0; //file size - make sure you replace with real size!
			res = 0; // no error

		*/
		//check if file
		else if(strcmp(filename, "") != 0){
			int i;
			int directory_location = -1;;
			int is_root = 1; //directory not found in root
			for(i = 0; i < root->nDirectories; i++){
				if (strcmp(directory, root->directories[i].dname) == 0) {
					//sub directory exisits
					// stbuf->st_mode = S_IFDIR | 0755;
					// stbuf->st_nlink = 2;
					// res = 0; //no error
					directory_location = i;
					is_root = 0;
					break;
				}
			}
			if(is_root){
					// stbuf->st_mode = S_IFDIR | 0755;
					// stbuf->st_nlink = 2;
					// printf("res = 0\n");
					// //no error
					// res = 0; 
				printf("-ENOENT1\n");
				res = -ENOENT;
			}
			else if(directory_location != -1){
				long starting_block = root->directories[directory_location].nStartBlock;

				cs1550_directory_entry *directory_entry;
				directory_entry = calloc(1, sizeof(cs1550_directory_entry));
				FILE *file = fopen(".disk", "rb");
				fseek(file, starting_block, SEEK_SET);
				fread(directory_entry, BLOCK_SIZE, 1, file);
				fclose(file);
					int j;
					for(j = 0; j < directory_entry->nFiles; j++){
						if (strcmp(filename, directory_entry->files[j].fname) == 0){
							if(strcmp(extension, directory_entry->files[j].fext) == 0){
								stbuf->st_mode = S_IFREG | 0666;
								stbuf->st_nlink = 1; //file links
								stbuf->st_size = 0; //file size - make sure you replace with real size!
								res = 0; // no error
							}
						}
					}
				free(directory_entry);
			}
			else{
				printf("-ENOENT2\n");
				res = -ENOENT;
			}
		}
		//Else return that path doesn't exist
		else{
			printf("-ENOENT3\n");
			res = -ENOENT;
		}
		free(root);
	}
	printf("end getattr\n");
	printf("res: %d\n", res);
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
 /*
DESCRIPTION
	This function should look up the input path,
	ensuring that it is a directory,
	and then list the contents.

	To list the contents,
	you need to use the filler() function.
	For example: filler(buf, ".", NULL, 0);
	adds the current directory to the listing generated by ls -a

	In general,
	you will only need to change the second parameter to
	be the name of the file or
	directory you want to add to the listing.
UNIX
	man -s 2 readdir
RETURN
	0 on success
	-ENOENT if the directory is not valid or found
 */
 //DUE DECEMBER 4
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	printf("start readattr\n");
	if(!bitmap_loaded){
		init_bitmap();
	}
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);

	//root
	if (strcmp(path, "/") == 0){
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory *root;
		root = calloc(1, sizeof(cs1550_root_directory));
		FILE *file = fopen(".disk", "rb");
		fread(root, sizeof(cs1550_root_directory), 1, file);
		fclose(file);

		int i;
		for(i = 0; i < root->nDirectories; i++){
			filler(buf, root->directories[i].dname, NULL, 0);
		}
		return 0;
	}

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	cs1550_root_directory *root;
	root = calloc(1, sizeof(cs1550_root_directory));
	FILE *file = fopen(".disk", "rb");
	fread(root, sizeof(cs1550_root_directory), 1, file);
	fclose(file);

	//check to see if directory exists
	int i;
	long dir_block = -1;
	for(i = 0; i < root->nDirectories; i++){
		if (strcmp(directory, root->directories[i].dname) == 0) {
			dir_block = root->directories[i].nStartBlock;
			break;
		}
	}
	if(dir_block == -1){
		return -ENOENT;
	}
	else{
		//the filler function allows us to add entries to the listing
		//read the fuse.h file for a description (in the ../include dir)
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		/*
		//add the user stuff (subdirs or files)
		//the +1 skips the leading '/' on the filenames
		filler(buf, newpath + 1, NULL, 0);
		*/
		cs1550_directory_entry *directory_entry;
		directory_entry = calloc(1, sizeof(cs1550_directory_entry));
		FILE *file = fopen(".disk", "rb");
		fseek(file, dir_block * BLOCK_SIZE, SEEK_SET);
		fread(directory_entry, BLOCK_SIZE, 1, file);
		fclose(file);
			int j;
			for(j = 0; j < directory_entry->nFiles; j++){
				char print[MAX_FILENAME + MAX_EXTENSION +1];
				strcpy(print, directory_entry->files[j].fname);
				if(strcmp(directory_entry->files[j].fext, "") != 0){
					strcat(print, ".");
					strcat(print, directory_entry->files[j].fext);
					filler(buf, print, NULL, 0);
				}
				else{
					filler(buf, print, NULL, 0);
				}
			}
		free(directory_entry);
	}
	printf("end readattr\n");
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
 /*
DESCRIPTION
	This function should add the new directory to the root level,
	and should update the .disk file appropriately.
UNIX
	man -s 2 mkdir
RETURN
	0 on success
	-ENAMETOOLONG if the name is beyond 8 chars
	-EPERM if the directory is not under the root dir only
	-EEXIST if the directory already exists
 */
 //DUE DECEMBER 4
static int cs1550_mkdir(const char *path, mode_t mode)
{
	printf("start mkdir\n");
	if(!bitmap_loaded){
		init_bitmap();
	}
	(void) path;
	(void) mode;

	//strings for directory name, filename, and file extension
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if(strlen(directory) >= MAX_FILENAME){
		printf("-ENAMETOOLONG\n");
		return -ENAMETOOLONG;
	}
	if(strcmp(filename, "") != 0){
		printf("-EPERM\n");
		return -EPERM;
	}

	cs1550_root_directory *root;
	root = calloc(1, sizeof(cs1550_root_directory));
	FILE *file = fopen(".disk", "r+b");
	fread(root, sizeof(cs1550_root_directory), 1, file);


	//check if directory is an existing subdirectory
	int i;
	for(i = 0; i < root->nDirectories; i++){
		//if yes, return error
		if (strcmp(directory, root->directories[i].dname) == 0) {
			fclose(file);
			free(root);
			printf("-EEXIST\n");
			return -EEXIST;
		}
	}

	//if not existing
	//load bitmap into memory and allocate an empty block for this directory
	//malloc new cs1550_directory_entry
	//add new link to this directory to the root block in memory
	//write updated bitmap, root block, and new block back to your ".disk" file
	int free_block = find_free_bit();
	cs1550_directory_entry *new_dir;
	new_dir = calloc(1, sizeof(cs1550_directory_entry));
	new_dir->nFiles = 0;

	int new_directory_loc;
	int k;
	for(k = 0; k < MAX_FILES_IN_DIR; k++){
		if(strcmp(root->directories[k].dname,"") == 0){
			new_directory_loc = k;
			break;
		}
	}
	root->nDirectories++;
	root->directories[new_directory_loc].nStartBlock = free_block;
	strcpy(root->directories[new_directory_loc].dname, directory);

	fseek(file, BLOCK_SIZE * root->directories[new_directory_loc].nStartBlock , SEEK_SET);
	//fwrite((void*)&root, sizeof(cs1550_directory_entry), 1, file);
	fwrite((void*)&new_dir, sizeof(cs1550_directory_entry), 1, file);
	rewind(file);
	fwrite((void*)&root, sizeof(cs1550_root_directory), 1, file);

	set_bit(free_block);

	fclose(file);
	free(new_dir);
	free(root);
	printf("end mkdir\n");
	return 0;
}

/*
 * Removes a directory.
 */
 //DUE DECEMBER 4
 //DO NOT MODIFY
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/******************************************************************************
 *
 *  DUE DECEMBER 11, 2016
 *	cs1550_mknod, cs1550_write, cs1550_read
 *****************************************************************************/
/*

 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
 /*
DESCRIPTION
	This function should add a new file to a subdirectory,
	and should update the .disk file appropriately with
	the modified directory entry structure.
UNIX
	man -s 2 mknod
RETURN
	0 on success
	-ENAMETOOLONG if the name is beyond 8.3 chars
	-EPERM if the file is trying to be created in the root dir
	-EEXIST if the file already exists
 */
 //DUE DECEMBER 11
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	printf("start mknod\n");
	(void) mode;
	(void) dev;

	if(!bitmap_loaded){
		init_bitmap();
	}

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if(strlen(filename) >= MAX_FILENAME){
		return -ENAMETOOLONG;
	}
	if(strcmp(filename, "") != 0){
		return -EPERM;
	}


	cs1550_root_directory *root;
	root = calloc(1, sizeof(cs1550_root_directory));
	FILE *file = fopen(".disk", "r+b");
	fread(root, sizeof(cs1550_root_directory), 1, file);


	//check if directory is an existing subdirectory
	int i;
	long dir_block = -1;
	for(i = 0; i < root->nDirectories; i++){
		if (strcmp(directory, root->directories[i].dname) == 0) {
			dir_block = root->directories[i].nStartBlock;
		}
	}

	//load bitmap into memory and allocate an empty block for this directory
	//malloc new cs1550_directory_entry
	//add new link to this directory to the root block in memory
	//write updated bitmap, root block, and new block back to your ".disk" file
	int free_block = find_free_bit();
	cs1550_directory_entry *dir;
	dir = calloc(1, sizeof(cs1550_directory_entry));
	fseek(file, BLOCK_SIZE * dir_block, SEEK_SET);
	fread(dir, sizeof(cs1550_directory_entry), 1, file);

	dir->nFiles++;
	dir->files[dir->nFiles].nStartBlock = free_block;
	strcpy(dir->files[dir->nFiles].fname, filename);
	strcpy(dir->files[dir->nFiles].fext, extension);
	dir->files[dir->nFiles].fsize = 0;


	//write directory to disk and free everything
	set_bit(free_block);
	
	fseek(file, BLOCK_SIZE * dir_block , SEEK_SET);
	fwrite((void*)&dir, sizeof(cs1550_directory_entry), 1, file);

	rewind(file);
	fwrite((void*)&root, sizeof(cs1550_root_directory), 1, file);

	set_bit(free_block);

	fclose(file);
	free(dir);
	free(root);

	printf("end mknod\n");
	return 0;
}

/*
 * Deletes a file
 */
 //DUE DECEMBER 11
 //DO NOT MODIFY
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
 /*
 DESCRIPTION
	 This function should read the data in the
	 file denoted by path into buf, starting at offset.
 UNIX
 	man -s 2 read
 RETURN
	 size read on success
	-EISDIR if the path is a directory
  */
 //DUE DECEMBER 11
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	printf("start read\n");
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	if(!bitmap_loaded){
		init_bitmap();
	}

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if(strcmp(filename, "") != 0){ //path is directory
		return -EPERM;
	}

	FILE *file = fopen(".disk", "r+b");

	cs1550_directory_entry *dir;
	cs1550_disk_block *readblock;
	readblock = calloc(1, sizeof(cs1550_disk_block));


	cs1550_root_directory *root;
	root = calloc(1, sizeof(cs1550_root_directory));
	fread(root, sizeof(cs1550_root_directory), 1, file);
	int i;
	long dir_block = -1;
	for(i = 0; i < root->nDirectories; i++){
		if (strcmp(directory, root->directories[i].dname) == 0) {
			dir_block = root->directories[i].nStartBlock;
		}
	}

	dir = calloc(1, sizeof(cs1550_directory_entry));
	fseek(file, BLOCK_SIZE * dir_block, SEEK_SET);
	fread(dir, sizeof(cs1550_directory_entry), 1, file);

	long file_block = -1;
	for(i = 0; i < dir->nFiles; i++){
		if (strcmp(filename, dir->files[i].fname) == 0) {
			file_block = dir->files[i].nStartBlock;
			fseek(file, BLOCK_SIZE * file_block + offset, SEEK_SET);
			size = fread((void*)&buf, size, 1, file);

		}
	}
	
	// fseek(file, BLOCK_SIZE * file_block, SEEK_SET);
	// fread(readblock, sizeof(cs1550_disk_block), 1, file);

	fclose(file);
	free(dir);
	free(root);
	free(readblock);
	printf("end read\n");
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
 /*
DESCRIPTION
	This function should write the data in buf
	into the file denoted by path, starting at offset.
UNIX
	man -s 2 write
RETURN
	size on success
	-EFBIG if the offset is beyond the file size (but handle appends)
 */
 //DUE DECEMBER 11
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	printf("start write\n");
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	if(!bitmap_loaded){
		init_bitmap();
	}
	if(offset > size){
		return -EFBIG;
	}

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	//empty out all strings
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	memset(extension, 0, MAX_EXTENSION + 1);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	FILE *file = fopen(".disk", "r+b");

	cs1550_directory_entry *dir;
	cs1550_disk_block *writeblock;
	writeblock = calloc(1, sizeof(cs1550_disk_block));


	cs1550_root_directory *root;
	root = calloc(1, sizeof(cs1550_root_directory));
	fread(root, sizeof(cs1550_root_directory), 1, file);
	int i;
	long dir_block = -1;
	for(i = 0; i < root->nDirectories; i++){
		if (strcmp(directory, root->directories[i].dname) == 0) {
			dir_block = root->directories[i].nStartBlock;
		}
	}

	dir = calloc(1, sizeof(cs1550_directory_entry));
	fseek(file, BLOCK_SIZE * dir_block, SEEK_SET);
	fread(dir, sizeof(cs1550_directory_entry), 1, file);

	long file_block = -1;
	for(i = 0; i < dir->nFiles; i++){
		if (strcmp(filename, dir->files[i].fname) == 0) {
			file_block = dir->files[i].nStartBlock;
			fseek(file, BLOCK_SIZE * file_block + offset, SEEK_SET);
			size = fwrite((void*)&buf, size, 1, file);
		}
	}

	// fseek(file, BLOCK_SIZE * file_block, SEEK_SET);
	// fread(writeblock, sizeof(cs1550_disk_block), 1, file);
	// fseek(file, BLOCK_SIZE * file_block + offset, SEEK_SET);
	// fwrite(buf, sizeof(buf), 1, file);

	fclose(file);
	free(dir);
	free(root);

	printf("end write\n");
	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
