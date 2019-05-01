//
// Simple FIle System
// Student Name : 정근화
// Student Number : B354025
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1 << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1 << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1 << (b)))
#define BIT_CHECK(a, b) ((a) & (1 << (b)))

static struct sfs_super spb;				// superblock
static struct sfs_dir sd_cwd = {SFS_NOINO}; // current working directory

void error_message(const char *message, const char *path, int error_code)
{
	switch (error_code)
	{
	case -1:
		printf("%s: %s: No such file or directory\n", message, path);
		return;
	case -2:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -3:
		printf("%s: %s: Directory full\n", message, path);
		return;
	case -4:
		printf("%s: %s: No block available\n", message, path);
		return;
	case -5:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -6:
		printf("%s: %s: Already exists\n", message, path);
		return;
	case -7:
		printf("%s: %s: Directory not empty\n", message, path);
		return;
	case -8:
		printf("%s: %s: Invalid argument\n", message, path);
		return;
	case -9:
		printf("%s: %s: Is a directory\n", message, path);
		return;
	case -10:
		printf("%s: %s: Is not a file\n", message, path);
		return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char *path)
{
	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read(&spb, SFS_SB_LOCATION);

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert(spb.sp_magic == SFS_MAGIC);

	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);

	sd_cwd.sfd_ino = 1; //init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount()
{

	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

void bitmapRead(){

	u_int8_t bits[SFS_BLOCKSIZE*CHAR_BIT];
	disk_read(bits, 2);
	int i, j;
	for(i = 0; i < SFS_BLOCKSIZE; i++){
		u_int8_t tp = bits[i];
		for(j = 0; j < CHAR_BIT; j++){
			printf("%d", tp%2);
			tp /= 2;
		}
		printf("\n");
	}

}

void sfs_touch(const char *path)
{
	//skeleton implementation

	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6
	// block 0: superblock,	block 1:root, 	block 2:bitmap
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined

	//buffer for disk read

	//===================================================
	// struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	// //block access
	// disk_read(sd, si.sfi_direct[0]);

	// //allocate new block
	// int newbie_ino = 6;

	// sd[2].sfd_ino = newbie_ino;
	// strncpy(sd[2].sfd_name, path, SFS_NAMELEN);

	// disk_write(sd, si.sfi_direct[0]);

	// si.sfi_size += sizeof(struct sfs_dir);
	// disk_write(&si, sd_cwd.sfd_ino);

	// struct sfs_inode newbie;

	// bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	// newbie.sfi_size = 0;
	// newbie.sfi_type = SFS_TYPE_FILE;

	// disk_write(&newbie, newbie_ino);

	// //+++ bitmap 정보도 수정해주어야함.

	bitmapRead();

}

int searchNode(const char *path){
	struct sfs_inode curNode;
	disk_read(&curNode, sd_cwd.sfd_ino);
	int i, j;
	int findPath = 0, iNodeNo;
	for(i = 0; i < SFS_NDIRECT; i++){
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if(curNode.sfi_direct[i] == 0 || findPath == 1)
			break;
		
		disk_read(dirs, curNode.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(dirs[j].sfd_ino == 0)
				break;
			if(strcmp(dirs[j].sfd_name, path) == 0){
				//find Path
				findPath = 1;
				iNodeNo = dirs[j].sfd_ino;
				break;
			}
		}
	}
	if(findPath == 0){
		//can't find
		return -1;
	}else{
		return iNodeNo;
	}

}

void sfs_cd(const char *path)
{
	//printf("Not Implemented\n");
	if(path == NULL){
		//change root directory
		sd_cwd.sfd_ino = SFS_ROOT_LOCATION;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}else{
		int iNodeNo = searchNode(path);
		if(iNodeNo == -1){
			//error can't find
			error_message("cd", path, -1);
		}else{
			//is file?
			struct sfs_inode tpInode;
			disk_read(&tpInode, iNodeNo);
			if(tpInode.sfi_type == SFS_TYPE_FILE){
				error_message("cd", path, -2);
				return;
			}
			//this is dirs
			sd_cwd.sfd_ino = iNodeNo;
			strcpy(sd_cwd.sfd_name, path);
		}
	}
}

void go_ls(const int iNodeNo, const char *path)
{
	struct sfs_inode curNode;
	disk_read(&curNode, iNodeNo);
	if(curNode.sfi_type == SFS_TYPE_FILE){
		printf("%s\n", path);
		return;
	}
	int i, j, eof = 0;
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		struct sfs_dir dirs[SFS_DENTRYPERBLOCK];
		if (curNode.sfi_direct[i] == 0)
		{
			if (eof)
				printf("\n");
			break;
		}
		disk_read(dirs, curNode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++)
		{
			if (dirs[j].sfd_ino == 0)
			{
				printf("\n");
				break;
				eof = 1;
			}
			struct sfs_inode iNode;
			printf("%s", dirs[j].sfd_name);
			disk_read(&iNode, dirs[j].sfd_ino);
			if (iNode.sfi_type == SFS_TYPE_DIR)
				printf("/");
			printf("\t");
		}
	}
}

void sfs_ls(const char *path)
{
	//printf("Not Implemented\n");

	if (path == NULL)
	{
		//root Path
		go_ls(sd_cwd.sfd_ino, NULL);
	}
	else
	{
		//find Path
		int iNodeNo = searchNode(path);
		if (iNodeNo == -1){
			//can't find coincidence path
			error_message("ls", path, -1);
		}
		else {
			//ls findPath
			go_ls(iNodeNo, path);
		}
	}
}

void sfs_mkdir(const char *org_path)
{
	printf("Not Implemented\n");
}

void sfs_rmdir(const char *org_path)
{
	printf("Not Implemented\n");
}

void sfs_mv(const char *src_name, const char *dst_name)
{
	printf("Not Implemented\n");
}

void sfs_rm(const char *path)
{
	printf("Not Implemented\n");
}

void sfs_cpin(const char *local_path, const char *path)
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char *local_path, const char *path)
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode)
{
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d", inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR)
	{
		for (i = 0; i < SFS_NDIRECT; i++)
		{
			if (inode.sfi_direct[i] == 0)
				break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}
}

void dump_directory(struct sfs_dir dir_entry[])
{
	int i;
	struct sfs_inode inode;
	for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
	{
		printf("%d %s\n", dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode, dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE)
		{
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump()
{
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n", sd_cwd.sfd_ino, sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");
}
