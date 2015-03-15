/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_stg.h
 **
 ** DESCRIPTION: This file contains definitions used by the overlay filesystem
 **              storage method.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 01/17/1998	ARTHUR N.	Added to source code control.
 ** 02/02/1998	ARTHUR N.	Added the Statfs() storage method operation
 **				 and added the option string to the Init()
 **				 operation.
 ** 02/06/1998	ARTHUR N.	Removed all "serving" of inodes from the
 **				 Storage System interface.
 ** 02/15/2003	ARTHUR N.	Pre-release of port to 2.4.
 ** 03/08/2003	ARTHUR N.	Added Get_mapping entry point.
 ** 03/11/2003	ARTHUR N.	Added Delete_dirent entry point.
 ** 03/11/2003	ARTHUR N.	Added flags argument to add_inode op.
 ** 03/26/2003	ARTHUR N.	Changed the Read_dir() entry point definition.
 ** 05/24/2003	ARTHUR N.	Added directory entry offsets.
 ** 07/05/2003	ARTHUR N.	Added storage flags.
 ** 07/08/2003	ARTHUR N.	Permanently use new directory handling.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef __ovl_stg_STDC_INCLUDE
#define __ovl_stg_STDC_INCLUDE	1

# ident "@(#)ovl_stg.h	1.12	[03/07/27 22:20:36]"

#include "ovl_block.h"
#include "ovl_types.h"


/* These must be defined even when STORE_MAPPINGS is set off. */
# define OVL_STG_FLAG__BASE_MAPPINGS	1
# define OVL_STG_FLAG__STG_MAPPINGS	2


/**
 ** NOTE: Although most of what is here is very similar to the structures
 **       used by the VFS, there are differences.  The most important
 **       difference between the overlay filesystem and regular filesystems
 **       is that the storage filesystem must track deleted files and changes
 **       to inodes that do not store their contents in the overlay filesystem.
 **/

/**
 ** NOTE: many of these are essentially the same as the super block or inode
 **       operations of similar name, except that these operations may assume
 **       direct access to their storage (no need to worry about redirecting
 **       operations), and they may assume that the inodes they are given
 **       have been validated for the filesystem.
 **
 ** The point of this structure is to allow more than one type of storage (such
 **  as a variable-size file, a fixed size file, or a block device) to be
 **  supported by a single module.
 **/



/******************************************************************************
 ******************************************************************************
 **
 ** TYPE DEFINITIONS
 **/

/**
 ** STRUCTURE: ovlfs_dir_info_struct
 **/

struct ovlfs_dir_info_struct {
	char		*name;
	int		len;
	_ulong		ino;
	int		flags;

	uint32_t	pos;	/* Used to keep consistenty - not persistent */
} ;

typedef struct ovlfs_dir_info_struct	ovlfs_dir_info_t;



/**
 ** STRUCTURE: ovlfs_storage_op_struct
 **
 ** DESCRIPTION:
 **	This structure defines all of the operations that must be supported by
 **	a storage mechanism in order to be used by the overlay filesystem.
 **/

struct ovlfs_storage_op_struct {
			/* Overall Maintenance Operations */
	int	(*Init)(struct super_block *, char *);
	int	(*Sync)(struct super_block *);
	int	(*Release)(struct super_block *);
	int	(*Statfs)(struct super_block *, struct statfs *, int);

			/* Inode Operations */
	int	(*Add_inode)(struct super_block *, _ulong, const char *, int,
		             _ulong *, int);
	int	(*Read_inode)(struct inode *, int *);
	int	(*Clear_inode)(struct inode *);
	int	(*Update_inode)(struct inode *);

			/* Inode Mapping Operations */
	int	(*Map_inode)(struct super_block *, _ulong, kdev_t, _ulong,
		             char);
	int	(*Map_lookup)(struct super_block *, kdev_t, _ulong,
		              _ulong *, char);
	int	(*Get_mapping)(struct super_block *, _ulong, char, kdev_t *,
		               _ulong *);

			/* Directory Content Operations */
	int	(*Num_dirent)(struct inode *, int);
	int	(*Read_dir)(struct inode *, loff_t *, ovlfs_dir_info_t **);
	int	(*Lookup)(struct inode *, const char *, int, _ulong *, int *);
	int	(*Add_dirent)(struct inode *, const char *, int, _ulong);
	int	(*Rename)(struct inode *, const char *, int, const char *, int);
	int	(*Unlink)(struct inode *, const char *, int);
	int	(*Delete_dirent)(struct inode *, const char *, int);

			/* File Content Operations */
	int	(*Read_file)(struct file *);
	int	(*Write_file)(struct file *);

			/* Symbolic Link Operations */
	int	(*Set_symlink)(struct inode *, char *, int);
	int	(*Read_symlink)(struct inode *, char *, int);
} ;

typedef struct ovlfs_storage_op_struct	ovlfs_storage_op_t;



/**
 ** STRUCTURE: ovlfs_StgFile_info_struct
 **
 ** ALIAS: Storage Method 1
 **
 ** DESCRIPTION:
 **	This structure contains the information needed to maintain inodes in
 **	the original "Storage File" concept.  In reality, most of the work is
 **	done in memory and updated in the storage file by writing the entire
 **	file's contents from scratch (very inefficient).
 **/

struct ovlfs_StgFile_info_struct {
	struct inode	*stg_inode;	/* Inode of the storage file */
	char		*stg_name;	/* Buffer holding name of stg file */
	int		stg_name_len;	/* Length of the value in stg_name */
	list_t		inodes;		/* List of inode info - in memory */
	list_t		base_to_ovl;	/* Map from base fs to ovl fs */
	list_t		stg_to_ovl;	/* Map from stg fs to ovl fs */

	int		tot_dirent;	/* Total # of dirents in use */
	int		tot_rm_dirent;	/* Total # of rm'ed dirents in use */
} ;

typedef struct ovlfs_StgFile_info_struct	ovlfs_StgFile_info_t;
typedef ovlfs_StgFile_info_t			ovlfs_stg1_t;	/* shorthand */



/**
 ** STRUCTURE: ovlfs_BlockedFile_info_struct
 **
 ** ALIAS: Storage Method 2
 **
 ** DESCRIPTION:
 **	This structure contains the information needed to maintain inodes in
 **	the "Blocked File" storage for the overlay filesystem.  This method is
 **	half-way between the Storage File concept and the Device concept;
 **	a file is used in an existing filesystem, but the file is accessed
 **	similar to a block device.  The advantages over a real Device include:
 **	(1) no need to allocate a device, (2) the file may grow or shrink, as
 **	appropriate, and (3) the contents are stored in a file which may be
 **	backed up and restored.
 **/

struct ovlfs_BlockedFile_info_struct {
	struct inode	*stg_inode;	/* Inode of the storage file */
	char		*stg_name;	/* Buffer holding path of stg file */
	int		stg_name_len;	/* Length of the value in stg_name */
	list_t		base_to_ovl;	/* Map from base fs to ovl fs */
	list_t		stg_to_ovl;	/* Map from stg fs to ovl fs */

	_uint		block_size;	/* Size of each block, in bytes */

	int		file_id;	/* File descriptor (see file_open) */
	char		file_opened_f;	/* Is the file open? */
} ;

typedef struct ovlfs_BlockedFile_info_struct	ovlfs_BlockedFile_info_t;
typedef ovlfs_BlockedFile_info_t		ovlfs_stg2_t;	/* shorthand */



/**
 ** STRUCTURE: ovlfs_BlockDevice_info_struct
 **
 ** ALIAS: Storage Method 3
 **
 ** DESCRIPTION:
 **	This structure contains the information needed to maintain inodes in
 **	the "Block Device" storage for the overlay filesystem.  This method is
 **	the same as used by standard filesystems.
 **/

struct ovlfs_BlockDevice_info_struct {
	kdev_t		dev_no;
	list_t		base_to_ovl;	/* Map from base fs to ovl fs */
	list_t		stg_to_ovl;	/* Map from stg fs to ovl fs */
} ;

typedef struct ovlfs_BlockDevice_info_struct	ovlfs_BlockDevice_info_t;
typedef ovlfs_BlockDevice_info_t		ovlfs_stg3_t;	/* shorthand */



/**
 ** STRUCTURE: ovlfs_dev_info_struct
 **
 ** DESCRIPTION:
 **	The ovlfs_dev_info_struct contains all of the needed information for
 **	any of the storage mechanisms used to maintain the persistent store
 **	of inode information for the overlay filesystem.
 **/

struct ovlfs_dev_info_struct {
	union {
		ovlfs_StgFile_info_t		stg1;
		ovlfs_BlockedFile_info_t	stg2;
		ovlfs_BlockDevice_info_t	stg3;
	} u;
} ;

typedef struct ovlfs_dev_info_struct	ovlfs_dev_info_t;



/**
 ** STRUCTURE: ovlfs_device_op_struct
 **
 ** NOTES:
 **	- Devices work on blocks, whether physical (all one size) units, or
 **	  logical units, blocks are identified by numbers.
 **
 **	- The idea here is to identify and isolate the physical interaction
 **	  from the logical interaction.  There will most likely be a
 **	  one-for-one correspondence of these structures with storage methods,
 **	  except for the Storage File method (perhaps).
 **
 ** RESPONSIBILITIES:
 **	method dev_Alloc()
 **		1. Add a block to the physical device, if available.
 **		2. Indicate when no more blocks are available.
 **
 **	method dev_Truncate()
 **		1. Reduce the usable space in the device.
 **
 **	method dev_Read()
 **		1. Read the block specified.
 **
 **	method dev_Write()
 **		1. Write the block specified.
 **
 **
 ** REQUIREMENTS:
 **	method dev_Alloc()
 **		1. Must be space available.
 **		2. Location of the buffer pointer must be given.
 **
 **	method dev_Truncate()
 **		1. The storage space can only be decreased.
 **
 **	method dev_Read()
 **		1. The buffer pointer must be valid.
 **
 **	method dev_Write()
 **		1. The buffer pointer must be valid.
 **/

struct ovlfs_device_op_struct {
	int	(*dev_Open)(ovlfs_dev_info_t *);
	int	(*dev_Close)(ovlfs_dev_info_t *);
	int	(*dev_Alloc)(ovlfs_dev_info_t *, ovlfs_block_t **);
	int	(*dev_Truncate)(ovlfs_dev_info_t *, _uint);
	int	(*dev_Read)(ovlfs_dev_info_t *, _uint, ovlfs_block_t *);
	int	(*dev_Write)(ovlfs_dev_info_t *, _uint, ovlfs_block_t *);
} ;

typedef struct ovlfs_device_op_struct	ovlfs_device_op_t;



/**
 ** STRUCTURE: ovlfs_storage_sys_struct
 **
 **   PURPOSE: Track a single Storage Method that is registered with the ovlfs.
 **
 ** ELEMENTS:
 **	 name		- name for the "system"; must be unique.
 **	 id		- locally assigned id.
 **	 ops		- pointer to the structure of storage methods.
 **	 use_count	- number of mounted fs' using the ops.
 **/

struct ovlfs_storage_sys_struct {
	char				*name;
	int				id;
	ovlfs_storage_op_t		*ops;
	int				use_count;
	struct ovlfs_storage_sys_struct	*next;
} ;

typedef struct ovlfs_storage_sys_struct	ovlfs_storage_sys_t;



/******************************************************************************
 ******************************************************************************
 **
 ** GLOBAL VARIABLES
 **/

extern ovlfs_storage_op_t	ovlfs_stg1_ops;

#endif
