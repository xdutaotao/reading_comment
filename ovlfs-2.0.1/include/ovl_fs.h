/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_fs.h
 **
 ** DESCRIPTION: This file contains definitions of constants, prototypes, and
 **              macros used by the overlay filesystem source files.
 **
 ** REVISION HISTORY
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 02/27/1998	ARTHUR N.	Initial checkin.
 ** 03/01/1998	ARTHUR N.	Added magic name options.
 ** 03/03/1998	ARTHUR N.	Added storage file inode reference to the
 **				 super block information and the
 **				 OVLFS_ALLOW_XDEV_RENAME compile option.
 ** 03/03/1998	ARTHUR N.	Added support for the noxmnt option.
 ** 03/04/1998	ARTHUR N.	Added the option to store mapping information
 **				 in the storage file.
 ** 03/05/1998	ARTHUR N.	Allocate all memory as KERNEL priority mem.
 ** 03/08/1998	ARTHUR N.	Added support for the VERIFY_MALLOC option.
 ** 03/10/1998	ARTHUR N.	Added support for the "updmntonly" option and
 **				 corrected a mistake in copy_inode_info().
 ** 03/10/1998	ARTHUR N.	Added use of the ovlfs_lock_inode in the
 **				 ovlfs_chown() function.
 ** 03/23/1998	ARTHUR N.	Many miscellaneous changes, including some
 **				 support for storage operations.
 ** 08/23/1998	ARTHUR N.	Added debug stack checking and removed
 **				 prototypes for functions that were made
 **				 static.
 ** 01/14/1998	ARTHUR N.	Added the stg_id to support registering of
 **				 storage methods, and added more comments.
 ** 02/02/1999	ARTHUR N.	Completed the changes for the storage method
 **				 interface.
 ** 02/03/1999	ARTHUR N.	Removed semicolon at the end of the IPUT macro
 **				 definition when KDEBUG_IREF is set.
 ** 02/06/1999	ARTHUR N.	Added the updated_ind and support for
 **				 debugging to a file instead of using printk.
 ** 02/06/1999	ARTHUR N.	Removed the updated_ind and some old code.
 **				 Also, restored locking.
 ** 02/15/2003	ARTHUR N.	Pre-release of port to 2.4.
 ** 02/24/2003	ARTHUR N.	Added PROC_DONT_IGET_F flag,
 **				 ovlfs_clear_inode_refs(), ovlfs_do_truncate(),
 **				 and ovlfs_inode2parent().  Also, removed
 **				 ovlfs_dir_dent from inode info struct.
 ** 03/06/2003	ARTHUR N.	Types and macros for iterative inode
 **				 resolution.
 ** 03/11/2003	ARTHUR N.	Added NO_BASE_REF inode flag.
 ** 03/11/2003	ARTHUR N.	Added prototypes for functions moved out of
 **				 ovl_ino.c.
 ** 03/11/2003	ARTHUR N.	Added prototype for do_create.
 ** 03/11/2003	ARTHUR N.	Added prototype for inode_refs_valid_p.
 ** 03/11/2003	ARTHUR N.	Added prototype for vfs_mkdir_noperm.
 ** 03/14/2003	ARTHUR N.	Replaced ovlfs_inode_get_child_dentry() and
 **				 ovlfs_inode_lookup_child_dentry() with a
 **				 unified version.
 ** 05/24/2003	ARTHUR N.	Added directory entry offset and index to sort
 **				 entries by offset.
 ** 06/08/2003	ARTHUR N.	Changed magic names from magic and magicr to
 **				 Magic and Magicr to prevent problems with the
 **				 /etc/magic file.
 ** 06/09/2003	ARTHUR N.	Corrected PRINK/DEBUG_PRINT macros.
 ** 07/08/2003	ARTHUR N.	Permanently use new directory handling.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef __ovl_STDC_INCLUDE
#define __ovl_STDC_INCLUDE	1

#ident "@(#)ovl_fs.h	1.25	[03/07/27 22:20:36]"

#include <linux/version.h>

#include "kernel_vers_spec.h"

#ifdef __KERNEL__
# include "kernel_lists.h"
#endif

#include "ovl_stg.h"


/******************************************************************************
 ******************************************************************************
 **
 ** CONSTANTS:
 **/


	/* Kernel version identifiers */

#if POST_20_KERNEL_F
# define USE_DENTRY_F		1
# define REF_DENTRY_F		1
# define INODE_FROM_INO20_DENT22(e)	( (e) == NULL ? NULL : (e)->d_inode )
#else
# define USE_DENTRY_F		0
# define INODE_FROM_INO20_DENT22(e)	(e)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,11)
# define HAVE_I_BLKBITS	1
#else
# define HAVE_I_BLKBITS	0
#endif

	/* handy shortcuts: */
#define INODE_NULL		(struct inode *) NULL
#define DENTRY_NULL		(struct dentry *) NULL


#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )
# define INODE_DIRTY_P(i)	( (i)->i_state & I_DIRTY )
# define INODE_MARK_DIRTY(i)	( (i)->i_state |= I_DIRTY )
# define INODE_CLEAR_DIRTY(i)	( (i)->i_state &= ~I_DIRTY )
# define LOCK_SUPER20(s)
# define UNLOCK_SUPER20(s)
#else
# define INODE_DIRTY_P(i)	( (i)->i_dirt )
# define INODE_MARK_DIRTY(i)	( (i)->i_dirt = 1 )
# define INODE_CLEAR_DIRTY(i)	( (i)->i_dirt = 0 )
# define LOCK_SUPER20(s)	lock_super(s)
# define UNLOCK_SUPER20(s)	unlock_super(s)
#endif

/**
 ** COMPILE-OPTION CONSTANTS
 **/

	/* OVLFS_DEFINE_BMP: if non-zero implement the bmp() inode op */
#ifndef OVLFS_DEFINE_BMP
# define OVLFS_DEFINE_BMP	1
#endif

#ifndef PROC_DONT_IGET_F
# define PROC_DONT_IGET_F	1
#endif

	/* Needed methods (stolen from kernel source): */

#define OVLFS_NEED_GET_SUPER		PRE_22_KERNEL_F

#define OVLFS_NEED_LOCK_INODE		1
#define OVLFS_NEED_UNLOCK_INODE		1
#define OVLFS_NEED_WAIT_ON_INODE	1
#define OVLFS_NEED_OPEN_INODE		1

#define OVLFS_NEED_DEF_PERM		PRE_22_KERNEL_F

	/* OVLFS_ALLOW_XDEV_RENAME: if a file is in the storage fs and the */
	/*  user renames it across a mount point (mv doesn't see the mount */
	/*  point since it is looking at the ovlfs), the pseudo fs will    */
	/*  allow the rename internally if this is set.                    */
	/* Regardless of the setting, the file in the storage fs will NOT  */
	/*  be moved across a mount point by the rename() operation.       */

#ifndef OVLFS_ALLOW_XDEV_RENAME
# define OVLFS_ALLOW_XDEV_RENAME	1
#endif

/**
 ** FS CONSTANTS
 **
 **	note that the *_INO constants needed to be distinct from those in the
 **	real fs', but that is NO LONGER so.
 **/

#define OVLFS_ROOT_INO		1
#define OVLFS_DIR_INO		0x01000001
#define OVLFS_FILE_INO		0x01000002
#define OVLFS_GENERIC_INO	0x01000003
#define OVLFS_SUPER_MAGIC	0xFE0F		/* Completely made up */

/**
 ** RUNTIME OPTION CONSTANTS
 **/

/* storage options: */
#define OVLFS_NO_STORAGE	0
#define OVLFS_USE_STORAGE	1
#define OVLFS_STG_CK_MEM	2	/* check available mem on alloc */
#define OVLFS_CROSS_MNT_PTS	4
#define OVLFS_STORE_BASE_MAP	8	/* save base fs mappings in stg file? */
#define OVLFS_STORE_STG_MAP	16	/* save stg fs mappings in stg file? */
#define OVLFS_UPD_UMOUNT_ONLY	32	/* only update storage at umount? */
#define OVLFS_NO_REF_UPD	64	/* don't set inode ref attributes */
#define OVLFS_NO_STORE_REFS	128	/* don't store refs in inode info */

#define OVLFS_STORE_ALL_MAPS	( OVLFS_STORE_BASE_MAP | OVLFS_STORE_STG_MAP )
#define OVLFS_STG_DEF_OPTS	( OVLFS_USE_STORAGE | OVLFS_CROSS_MNT_PTS | \
				  OVLFS_STORE_ALL_MAPS )


	/***
	 *** MAGIC options: set USE_MAGIC to non-zero int on the compile
         ***  line to compile in the source that allows the use of the magic
         ***  directories.  If compiled in, a mount option can be used to
	 ***  indicate whether to turn the magic directories on or off.
	 ***/

#if USE_MAGIC
	/* magic options */
# define OVLFS_NO_MAGIC		0
# define OVLFS_BASE_MAGIC	1
# define OVLFS_OVL_MAGIC	2
# define OVLFS_HIDE_MAGIC	4
# define OVLFS_ALL_MAGIC	( OVLFS_BASE_MAGIC | OVLFS_OVL_MAGIC | \
				  OVLFS_HIDE_MAGIC )

# define OVLFS_MAGIC_NAME	"Magic"
# define OVLFS_MAGIC_NAME_LEN	5

# define OVLFS_MAGICR_NAME	"Magicr"
# define OVLFS_MAGICR_NAME_LEN	6
#endif

/**
 ** OVLFS Storage CONSTANTS.
 **/

	/* flags for ovlfs_inode_struct */
#define OVLFS_INODE_READ	1	/* Has inode been "read" yet? */

	/* flags for ovlfs_dir_info */
#define OVLFS_DIRENT_NORMAL	0	/* No special info */
#define OVLFS_DIRENT_UNLINKED	1	/* Directory entry was removed */
#define OVLFS_DIRENT_RELINKED	2	/* Entry was removed then recreated */


/**
 ** DEBUGGING CONSTANTS
 **
 ** NOTES:
 **	- Debug messages are printed using printk() and can be viewed by
 **	  looking at a console text screen or by running dmesg.
 **
 **	- KDEBUG's meanings are as follows:
 **		- if not defined (#undef KDEBUG), no debug messages will be
 **		  generated by the ovlfs regardless of the severity.
 **
 **		- if defined to 0 or non-numeric, only serious erros will
 **		  generate debug messages.
 **
 **		- if set to 1, warnings will generate debug messages in
 **		  addition to the serious error messages.
 **
 **		- if set above 1, be prepared for inundation of debugging
 **		  information.  It is HIGHLY recommended that this NEVER be
 **		  used unless absolutely necessary.
 **/

#define OVLFS_DEBUG_FILE	"/tmp/ovlfs.debug"
#define KDEBUG_CALL_PREFIX	"OVLFS: CALL: "
#define KDEBUG_RET_PREFIX	"OVLFS: RETURN: "
#ifndef KDEBUG_CALLS
# if KDEBUG > 8
#  define KDEBUG_CALLS 1
# endif
#endif

	/* lock/unlock debugging */
#define ovlfs_lock_inode0(i)	\
	printk("OVLFS: lock inode 0x%x @ %s: %d\n", (int) (i), __FILE__, \
	       __LINE__), \
	ovlfs_lock_inode(i)

#define ovlfs_unlock_inode0(i)	\
	printk("OVLFS: unlock inode 0x%x @ %s: %d\n", (int) (i), __FILE__, \
	       __LINE__),\
	ovlfs_unlock_inode(i)


/**
 ** Overlay Filesystem inode flags
 **
 **  Here is a brief description of each (only identified by its suffix):
 **
 **	SIZE_LIMIT	- The inode size known by ovlfs is the hard limit for
 **			  reading when this is set; otherwise, the size is
 **			  essentially ignored and all data read from the real
 **			  inodes is returned (likewise, data not read is not
 **			  returned in the case of larger sizes).
 **
 **	NO_BASE_REF	- Base filesystem references are not used by this
 **			  inode; this is caused when a directory entry with a
 **			  base reference is removed, and later re-added.
 **/

#define OVL_IFLAG__SIZE_LIMIT	1
#define OVL_IFLAG__NO_BASE_REF	2


/**
 ** Inode resolution flags
 **/

#define OVLFS_FOLLOW_MOUNTS	1
#define OVLFS_MAKE_HIER		2
#define OVLFS_MAKE_LAST		4


/**
 ** Reference Attachment Flags (see attach_inode_reference).
 **/

#define OVLFS_NO_REF_ID_UPD_F	1
#define OVLFS_NO_REF_ATTACH_F	2
#define OVLFS_NO_REF_MAP_F	4



/**
 ** Dentry retrieval flags
 **/

#define OVLFS_DENT_GET_POSITIVE	1
#define OVLFS_DENT_GET_NEGATIVE	2
#define OVLFS_DENT_GET_ANY	( OVLFS_DENT_GET_POSITIVE | \
				  OVLFS_DENT_GET_NEGATIVE )


/******************************************************************************
 ******************************************************************************
 **
 ** TYPE DEFINITIONS:
 **/

/* syscall_t is a function pointer used by interfacing to system calls */
typedef int		(*syscall_t)();

/* Shortcuts: trying to make these distinct from any in use */

#ifndef UNSIGNED_SHORTCUTS
# define UNSIGNED_SHORTCUTS	1
typedef unsigned int	_uint;
typedef unsigned long	_ulong;
typedef unsigned char	_uchar;
#endif


/**
 ** STRUCTURE: ovlfs_super_info_struct
 **
 ** DESCRIPTION: Maintain the information and options for an overlayed
 **              filesystem.
 **
 ** MEMBERS:
 **	base_root	- root directory of the filesystem to overlay (i.e. the
 **			  original/real filesystem).
 **	storage_root	- root directory of the area to store changes to the
 **			  overlayed fs.
 **	root_ent	- the inode, or dentry, of the base filesystems root
 **			  directory.
 **	storage_ent	- the inode, or dentry, of the storage fs's root
 **			  directory.
 **	storage_opts	- options for storing changes to the overlayed fs.
 **	magic_opts	- options for turning on or off the use of the magic
 **			  files.
 **	u		- union of storage-method specific information.
 **	stg_id		- internal identifier for the storage method used by
 **			  the mounted filesystem.
 **	stg_ops		- pointer to the vector table of storage method
 **			  operations for this filesystem's storage method.
 **/

struct ovlfs_super_info_struct {
	char		*base_root;
	char		*storage_root;
#if USE_DENTRY_F
	struct dentry	*root_ent;
	struct dentry	*storage_ent;
#else
	struct inode	*root_ent;
	struct inode	*storage_ent;
#endif
	char		storage_opts;
#if USE_MAGIC
	char		magic_opts;
	char		*Smagic_name;	/* Name of magic dir for the STG FS */
	char		*Bmagic_name;	/* Name of magic dir for the Base FS */
	int		Smagic_len;
	int		Bmagic_len;
#endif

	union {
		ovlfs_StgFile_info_t	stg1;
		void			*generic;
	} u;

	int			stg_id;
	ovlfs_storage_op_t	*stg_ops;
} ;

typedef struct ovlfs_super_info_struct	ovlfs_super_info_t;




/**
 ** STRUCTURE: ovlfs_inode_special_struct
 **
 ** DESCRIPTION:
 **	This structure contains the extra information for overlay filesystem
 **	inodes that is needed for each inode.
 **
 ** MEMBERS:
 **	parent_ino	- The inode number of the directory (at least one
 **			  of...) of this inode.  This is needed for creating
 **			  the directory tree to an inode in the storage fs.
 **	base_dev	- The device of the inode in the base filesystem that
 **			  the pseudo filesystem's inode refers to.
 **	base_ino	- The inode number of the inode in the base filesystem
 **			  that the pseudo filesystem's inode refers to.
 **	stg_dev		- The device of the inode in the storage filesystem
 **			  that the pseudo filesystem's inode refers to.
 **	stg_ino		- The inode number of the inode in the storage file-
 **			  system that the pseudo filesystem's inode refers to.
 **	flags		- Inode status flags for the ovlfs, such as the
 **			  SIZE_LIMIT flag.
 **	name		- The name of the inode - used when it must be copied
 **			  from the base filesystem to the storage filesystem.
 **	len		- Length of the name of the inode.
 **	updated_ind	- Flag indicating whether the inode's content
 **			  information has been updated.  Needed by directories.
 **/

struct ovlfs_inode_special_struct {
	_ulong	parent_ino;	/* At least, one of the parents ;) */
	kdev_t	base_dev;
	_ulong	base_ino;
	kdev_t	stg_dev;
	_ulong	stg_ino;
	int	flags;

	char	*name;		/* At least, one of the names ;) */
	int	len;
} ;

typedef struct ovlfs_inode_special_struct	ovlfs_inode_special_t;



/**
 ** STRUCTURE: ovlfs_inode_struct
 **
 ** DESCRIPTION: Maintain the information for the overlay filesystem inode.
 **/

struct ovlfs_inode_struct {
	struct super_block	*sb;

	umode_t			mode;
	off_t			size;
	uid_t			uid;
	gid_t			gid;
	time_t			atime;
	time_t			mtime;
	time_t			ctime;
	nlink_t			nlink;
	unsigned long		blksize;
	unsigned long		blocks;
	unsigned long		version;
	unsigned long		nrpages;

	union {
		kdev_t	rdev;	/* for device special files */
		char	*link;	/* for symbolic links */
		void	*generic;
		int	int_value;
	} u;

	_ulong			ino;

	ovlfs_inode_special_t	s;

		/* Number of OVLFS_DIRENT_UNLINKED dir entries: */
	int			num_rm_ent;

	list_t			dir_entries;

		/* For consistency; not persistent. */
	uint32_t		last_dir_pos;
	int			pos_index;

		/* Are the inode attributes valid? */
	int			att_valid_f;
} ;

typedef struct ovlfs_inode_struct   ovlfs_inode_t;



/**
 ** STRUCTURE: ovlfs_inode_info
 **
 ** PURPOSE: This structure holds information for a single inode in the overlay
 **          filesystem.
 **/

struct ovlfs_inode_info {
#if STORE_REF_INODES
# if USE_DENTRY_F && REF_DENTRY_F
	struct dentry	*base_dent;
	struct dentry	*overlay_dent;
# else
	struct inode	*ovlfs_dir;	/* for files, directory they are in */
	struct inode	*base_inode;
	struct inode	*overlay_inode;
# endif
#else
	char		*base_name;
	char		*overlay_name;
#endif

#if POST_20_KERNEL_F
	struct semaphore	page_io_file_sem;
	char			page_file_which;
	struct file		*page_io_file;
#endif

	ovlfs_inode_special_t	s;
} ;

typedef struct ovlfs_inode_info	ovlfs_inode_info_t;



/**
 ** STRUCTURE: ovlfs_file_info_struct
 **
 **   PURPOSE: Maintain information on a file that references an ovlfs inode.
 **/

struct ovlfs_file_info_struct {
	struct file	*file;
	char		is_base;
} ;

typedef struct ovlfs_file_info_struct	ovlfs_file_info_t;



/**
 ** STRUCTURE: ovlfs_externs_struct
 **
 **   PURPOSE: Maintain the references to all functions and data that are made
 **            externally available for the overlay filesystem.  The concept
 **            is to maintain as few actual external symbols as possible.
 **
 ** NOTES:
 **	- I am debating whether to do this or not since I have multiple source
 **	  files, and to export functions and variables that are static to
 **	  each one, I would need to setup the structure at load time by
 **	  calling one function for each source file (yet more externals!).
 **/

struct ovlfs_externs_struct {
} ;

typedef struct ovlfs_externs_struct	ovlfs_externs_t;



/**
 ** STRUCTURE: ovlfs_ino_map_struct
 **
 ** DESCRIPTION: Maintain mapping from a device number and inode number to
 **              the inode number of the overlay fs inode that it belongs to.
 **/

struct ovlfs_ino_map_struct {
	kdev_t	dev;
	_uint	ino;
	_uint	ovl_ino;
} ;

typedef struct ovlfs_ino_map_struct	ovlfs_ino_map_t;



/**
 ** TYPE: ref_hist_t
 ** TYPE: walk_info_t
 **
 ** DESCRIPTION:
 **	These are used to enable iterative walking of the directory tree when
 **	resolving inode references.  See ovlfs_resolve_inode().
 **/

typedef struct ref_hist_struct	ref_hist_t;

struct ref_hist_struct {
	struct inode	*inode;
	ref_hist_t	*descendent;
} ;


typedef struct walk_info_struct	walk_info_t;
struct walk_info_struct {
	struct inode	*inode;
	char		which;
#if POST_20_KERNEL_F
	struct dentry	*cur_ancestor;
#else
# error "need to update for 2.0"
#endif
	ref_hist_t	*history;
	int		flags;
} ;



/******************************************************************************
 ******************************************************************************
 **
 ** PROTOTYPES:
 **/

#ifdef __cplusplus
extern "C" {
#endif

#define dprintf	ovlfs_dprintf

	extern struct inode	*do_iget(struct super_block *, _ulong, int);

	extern void	ovlfs_end_debug();
	extern void	ovlfs_dprintf(int, char *, ...);
	extern void	ovlfs_debugf(char *, ...);
	extern int	ovlfs_resolve_inode(struct inode *, struct inode **,
			                    char);
#if USE_DENTRY_F
	extern int	ovlfs_resolve_dentry(struct inode *, struct dentry **,
			                     char);
	extern int	ovlfs_resolve_dentry_hier(struct inode *,
			                          struct dentry **, char, int);
#endif
	extern int	inode_refs_valid_p(struct inode *);
	extern void	ovlfs_debug_inodes(struct inode *);

	extern void	debug_prep_stack_usage();
	extern void	debug_calc_stack_usage();

#if USE_DENTRY_F
	extern int	ovlfs_copy_file(struct inode *, const char *, int,
			                off_t, struct inode *,
			                struct dentry **);
	extern int	ovlfs_inode_copy_file(struct inode *, struct inode *,
			                      struct dentry **);
#else
	extern int	ovlfs_copy_file(struct inode *, const char *, int,
			                off_t, struct inode *,
			                struct inode **);
	extern int	ovlfs_inode_copy_file(struct inode *, struct inode *,
			                      struct inode **);

	extern int	ovlfs_attach_reference(struct inode *, struct dentry *,
			                        char, int);
#endif

	extern int	ovlfs_make_hierarchy(struct inode *, struct inode **,
			                     int);

	extern struct inode	*ovlfs_get_parent_dir(struct inode *);

/* Directory operations */
	int		ovlfs_read_directory(struct inode *);
	int		update_dir_entry(struct inode *, const char *, int,
			                 _ulong *);

/* Storage operations */
	extern ovlfs_inode_t	*ovlfs_add_inode(struct super_block *,
				                 const char *, int);
	extern int		ovlfs_inode_set_base(struct super_block *,
				                     ovlfs_inode_t *, dev_t,
				                     _uint);
	extern int		ovlfs_inode_set_stg(struct super_block *,
				                    ovlfs_inode_t *, dev_t,
				                    _uint);
	extern ovlfs_inode_t	*ovlfs_get_ino(struct super_block *, _ulong);
	extern _ulong		ovlfs_lookup_ino(struct super_block *, kdev_t,
				                 _ulong, char);
	extern int		ovlfs_ino_add_dirent(ovlfs_inode_t *,
				                     const char *, int, _ulong,
				                     int);
	extern ovlfs_dir_info_t	*ovlfs_ino_find_dirent(ovlfs_inode_t *,
				                       const char *, int);
	extern int		ovlfs_remove_dirent(ovlfs_inode_t *,
				                    const char *, int);
	extern int		ovlfs_unlink_dirent(ovlfs_inode_t *,
				                    const char *, int);

	void			release_page_io_file(ovlfs_inode_info_t *);

/**
 ** Kernel "replacements":
 **/

# if OVLFS_NEED_GET_SUPER
	extern struct super_block	*ovlfs_get_super(kdev_t);
# endif
# if OVLFS_NEED_OPEN_INODE
	int				ovlfs_open_dentry(struct dentry *, int,
					                  struct file **);
	int				ovlfs_open_inode(struct inode *, int,
					                 struct file **);
	void				ovlfs_close_file(struct file *);
# endif

		/* Inode locking operations */
# if OVLFS_NEED_WAIT_ON_INODE
	extern int		ovlfs_wait_on_inode(struct inode *);
# endif
# if OVLFS_NEED_LOCK_INODE
	extern int		ovlfs_lock_inode(struct inode *);
# endif
# if OVLFS_NEED_UNLOCK_INODE
	extern void		ovlfs_unlock_inode(struct inode *);
# endif

		/* Permission checking */

# if OVLFS_NEED_DEF_PERM
	extern int		ovlfs_ck_def_perm(struct inode *, int);
# endif

		/* System call interface setup */
	void	ovlfs_setup_syscalls();

#if VERIFY_MALLOC
	void	ovlfs_start_alloc(const char *);
	void	ovlfs_stop_alloc(const char *);
	void	*ovlfs_alloc(unsigned int);
	void	ovlfs_free(const void *);
	void	ovlfs_free2(const void *, unsigned int);
#endif

#if POST_20_KERNEL_F

	void	ovlfs_clear_inode_refs(struct inode *);

	struct dentry	*ovlfs_inode2dentry(struct inode *);
	struct dentry	*ovlfs_inode2parent(struct inode *);

	int	ovlfs_inode_get_child_dentry(struct inode *, const char *, int,
		                             struct dentry **, int);

	int	inode_get_named_dent(struct inode *, const char *, int,
		                     struct dentry *, struct dentry **);

	char	*get_dentry_full_path(struct dentry *);
	int	get_inode_vfsmount(struct inode *, struct vfsmount **);
	int	ovlfs_dentry_follow_down(struct dentry *, struct dentry **,
		                         struct super_block *);

	int	ovlfs_do_truncate(struct dentry *, unsigned int);

#else 

	int	ovlfs_do_truncate(struct inode *, unsigned int);

#endif

#if POST_20_KERNEL_F
	int	vfs_mkdir_noperm(struct inode *, struct dentry *, int);
#endif

	int	do_mkdir(struct inode *, const char *, int, int);
	int	do_link(struct inode *, struct inode *, const char *, int);
	int	do_unlink (struct inode *, const char *, int);
	int	do_rmdir (struct inode *, const char *, int);
	int	do_lookup(struct inode *, const char *, int, struct inode **,
		          int);
#if POST_20_KERNEL_F
	int	do_lookup2 (struct inode *, const char *, int,
		            struct dentry **, int);
	int	do_lookup3(struct dentry *, const char *, int,
		           struct dentry **, int);
#endif
	int	do_rename(struct inode *, const char *, int, struct inode *,
		          const char *, int, int);
	int	do_symlink(struct inode *, const char *, int, const char *);
#if POST_20_KERNEL_F
	int	do_readlink(struct dentry *, char *, int);
#else
	int	do_readlink(struct inode *, char *, int);
#endif
	int	do_mknod(struct inode *, const char *, int, int, int);
#if POST_20_KERNEL_F
	int	do_create(struct inode *, const char *, int, int,
		          struct dentry **);
#else
	int	do_create(struct inode *, const char *, int, int,
	                   struct inode **);
#endif

#ifdef __cplusplus
}
#endif



/******************************************************************************
 ******************************************************************************
 **
 ** EXTERNAL DECLARATIONS:
 **
 **	note that these are not all placed into the kernel symbol table.
 **/

extern struct dentry_operations		ovlfs_dentry_ops;
extern struct file_operations		ovlfs_dir_fops;
extern struct inode_operations		ovlfs_ino_ops;
#if POST_22_KERNEL_F
extern struct inode_operations		ovlfs_symlink_ino_ops;
#else
extern struct inode_operations		ovlfs_file_ino_ops;
#endif
extern struct address_space_operations	ovlfs_addr_ops;
extern struct file_operations		ovlfs_file_ops;

extern syscall_t	open;
extern syscall_t	close;
extern syscall_t	read;
extern syscall_t	lseek;
extern syscall_t	write;




/******************************************************************************
 ******************************************************************************
 **
 ** MACROS:
 **/

/* Macro to convert a #defined value to a string (i.e. put quotes around it) */
#define do_mk_str(x)	#x
#define macro2str(x)	do_mk_str(x)

/* The third parameter to open_namei() is the mode of the file; since we */
/*  are not creating files (yet?) this is set to 0.                      */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0)
# define get_inode(f,i)		open_namei(f, 0, 0, i, (struct inode *) NULL)
# define get_inode_ro(f,i)	open_namei(f, 1, 0, i, (struct inode *) NULL)
# define get_inode_wo(f,i)	open_namei(f, 2, 0, i, (struct inode *) NULL)
# define get_inode_rw(f,i)	open_namei(f, 3, 0, i, (struct inode *) NULL)
#else

	int	get_inode_mode(const char *, int, struct inode **);

# define get_inode(f,i)		get_inode_mode(f, 0, i)
# define get_inode_ro(f,i)	get_inode_mode(f, 1, i)
# define get_inode_wo(f,i)	get_inode_mode(f, 2, i)
# define get_inode_rw(f,i)	get_inode_mode(f, 3, i)

#endif

#define opt_use_storage_p(o)	((o).storage_opts & OVLFS_USE_STORAGE)

#if USE_DENTRY_F
# define ovlfs_resolve_base_dentry(i,r)	ovlfs_resolve_dentry((i),(r),'b')
# define ovlfs_resolve_ovl_dentry(i,r)	ovlfs_resolve_dentry((i),(r),'o')
# define ovlfs_resolve_stg_dentry(i,r)	ovlfs_resolve_ovl_dentry((i),(r))
#endif

#define ovlfs_resolve_base_inode(i,r)	ovlfs_resolve_inode((i),(r),'b')
#define ovlfs_resolve_ovl_inode(i,r)	ovlfs_resolve_inode((i),(r),'o')
#define ovlfs_resolve_stg_inode(i,r)	ovlfs_resolve_ovl_inode((i),(r))

#define ovlfs_get_super_opts(sb)	\
	((sb) == NULL? NULL :  (sb)->u.generic_sbp )
#define ovlfs_sb_opts(sb)		\
	((ovlfs_super_info_t *)((sb) == NULL? NULL : (sb)->u.generic_sbp))
#define ovlfs_magic_opt_p(sb,m)					\
	((sb) == NULL? 0 : ( (sb)->u.generic_sbp == NULL ?	\
	                     0 : ovlfs_sb_opts(sb)->magic_opts & (m)))
#define ovlfs_sb_opt(sb,opt)			\
	( (sb) == NULL?				\
	  0 : ( (sb)->u.generic_sbp == NULL ?	\
	        0 : ((ovlfs_super_info_t *) (sb)->u.generic_sbp)->opt ) )
#define ovlfs_sb_xmnt(sb)		\
	( ( ovlfs_sb_opt((sb), storage_opts) & OVLFS_CROSS_MNT_PTS ) ? 1 : 0 )

#define is_ovlfs_inode(i)	(((i)->i_sb != (struct super_block *) NULL) &&\
				 ((i)->i_sb->s_magic == OVLFS_SUPER_MAGIC))

 /* Get the ovlfs inode info for the given inode: use this macro because the */
 /*  information structure's pointer will be put into the inode's info.      */

#define ovlfs_inode_get_ino(i)	( (i) == NULL? NULL : \
				  ovlfs_get_ino((i)->i_sb, (i)->i_ino) )

/**
 ** MACRO: copy_inode_info
 **
 ** PURPOSE: Copy the inode information that is "copy-able" from the given
 **          source inode to the given target inode.
 **
 ** NOTES:
 **	- Do NOT copy the inode number.  Once an inode is obtained with iget,
 **	  its inode number must not change until it is iput for the last time.
 **	- Do not upate rdev here - it is updated where appropriate.
 **	- It appears some versions of the kernel used an entry called i_bytes
 **	  for quotas, but others don't have them.  Either way, don't copy -
 **	  quota data should be maintained for each inode on its own regardless.
 **/

#define copy_inode_info_common(t,f,o)			\
	(void)						\
	(						\
		(t)->i_mode    = (f)->i_mode,		\
		(t)->i_nlink   = (f)->i_nlink,		\
		(t)->i_uid     = (f)->i_uid,		\
		(t)->i_gid     = (f)->i_gid,		\
		(t)->i_size    = (f)->i_size,		\
		(t)->i_atime   = (f)->i_atime,		\
		(t)->i_ctime   = (f)->i_ctime,		\
		(t)->i_mtime   = (f)->i_mtime,		\
		(t)->i_blksize = (f)->i_blksize,	\
		(t)->i_blocks  = (f)->i_blocks,		\
		(t)->i_version = (f)->i_version,	\
		(t)->i_version = (f)->i_version,	\
		o					\
	)

#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0) )
# define copy_inode_info(t,f)				\
	copy_inode_info_common((t),(f),(t)->i_nrpages = (f)->i_nrpages)
#else
# if HAVE_I_BLKBITS
#  define copy_inode_info(t,f)	\
	copy_inode_info_common((t),(f),(t)->i_blkbits = (f)->i_blkbits)
# else
#  define copy_inode_info(t,f)	\
	copy_inode_info_common((t),(f),0)
# endif
#endif


/* Inode storage macros */

#define create_ino_map()    (ovlfs_ino_map_t *) MALLOC(sizeof(ovlfs_ino_map_t))

#define ovlfs_lookup_base_ino(s,d,i)	ovlfs_lookup_ino((s), (d), (i), 'b')
#define ovlfs_lookup_stg_ino(s,d,i)		ovlfs_lookup_ino((s), (d), (i), 's')


/* file info stuff */
#define file_info_filp(i)	((i) == NULL ? (struct file *) NULL : \
				               (i)->file)
#define file_info_is_base(i)	((i) == NULL ? 0 : (i)->is_base)


/* DEBUGGING */

#if PRINTK_USE_DEBUG
# define PRINTK	ovlfs_debugf
#else
# define PRINTK	printk
#endif

#if KDEBUG > 1  ||  KDEBUG_IREF
# define IPUT(i)	dprintf(9, "OVLFS: file %s, line %d: putting inode " \
			        "%ld at 0x%x count before = %d\n", \
			        __FILE__, __LINE__, \
			        (i)->i_ino, (i), (i)->i_count), iput(i)
#else
# define IPUT(i)	iput(i)
#endif

#if POST_20_KERNEL_F
	/* While igrab() is the more correct choice here, it is not needed */
	/*  since there is no chance this macro is used on an inode whose  */
	/*  count may decrease below 1 before it completes.                */
# define INC_ICOUNT(i)	atomic_inc(&((i)->i_count))
#else
# define INC_ICOUNT(i)	(i)->i_count++
#endif

#if KDEBUG > 1  ||  KDEBUG_IREF
# define IMARK(i)	dprintf(9, "OVLFS: file %s, line %d: marking inode " \
			        "%ld at 0x%x; count before = %d\n", \
			        __FILE__, __LINE__, \
			        (i)->i_ino, (i), (i)->i_count), INC_ICOUNT(i)
#else
# define IMARK(i)	INC_ICOUNT(i)
#endif


#if defined(KERNEL_VERSION) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0) )
# define IPUT20(i)
# define IMARK20(i)
#else
# define IPUT20(i)	IPUT(i)
# define IMARK20(i)	IMARK(i)
#endif

#if KDEBUG > 1  ||  KDEBUG_SEMA
# define DOWN(s)	dprintf(9, "OVLFS: file %s, line %d: down on " \
			        "semaphore at 0x%x; count before = %d\n", \
			        __FILE__, __LINE__, \
			        (s), atomic_read(&((s)->count))), down(s)
# define UP(s)		dprintf(9, "OVLFS: file %s, line %d: up on " \
			        "semaphore at 0x%x; count before = %d\n", \
			        __FILE__, __LINE__, \
			        (s), atomic_read(&((s)->count))), up(s)
#else
# define DOWN(s)	down(s)
# define UP(s)		up(s)
#endif


/** DEBUG_SRC# - include source if debug level is >= # **/

#ifndef DEBUG_SRC1
# if KDEBUG
#  define DEBUG_SRC1(s)	s
# else
#  define DEBUG_SRC1(s)	
# endif
#endif

#ifndef DEBUG_SRC2
# if KDEBUG > 1
#  define DEBUG_SRC2(s)	s
# else
#  define DEBUG_SRC2(s)
# endif
#endif

/* This is not too clean; it is intended to help detect corruption */
#ifndef DEBUG_STACK
# if DEBUG_CHECK_STACK
#  define DEBUG_STACK	ovlfs_check_kernel_stack(__FILE__, __LINE__,	\
			                         "debug call")
#  define DEBUG_STACK_COMMA	,
# else
#  define DEBUG_STACK
#  define DEBUG_STACK_COMMA
# endif
#endif

#ifndef DEBUG_CALLS_PRINT
# if KDEBUG_CALLS
#  define DEBUG_CALLS_PRINT(s)		DEBUG_STACK DEBUG_STACK_COMMA PRINTK s
#  define DEBUG_CALLS_PRINT2(args...)					\
		DEBUG_STACK DEBUG_STACK_COMMA				\
		PRINTK(KERN_INFO KDEBUG_CALL_PREFIX "%s [%s, %u]: ",	\
		       __FUNCTION__, __FILE__, __LINE__),		\
		PRINTK(args)
#  define DEBUG_RETS_PRINT2(args...)					\
		DEBUG_STACK DEBUG_STACK_COMMA				\
		PRINTK(KERN_INFO KDEBUG_RET_PREFIX "%s [%s, %u]: ",	\
		       __FUNCTION__, __FILE__, __LINE__),		\
		PRINTK(args)
# else
#  define DEBUG_CALLS_PRINT(s)		DEBUG_STACK
#  define DEBUG_CALLS_PRINT2(s...)	DEBUG_STACK
#  define DEBUG_RETS_PRINT2(s...)	DEBUG_STACK
# endif
#endif

#ifndef DEBUG_PRINT1
# if KDEBUG
#  define DEBUG_PRINT1(s)	PRINTK s
# else
#  define DEBUG_PRINT1(s)
# endif
#endif

#ifndef DEBUG_PRINT5
# if KDEBUG > 4
#  define DEBUG_PRINT5(s)	PRINTK s
# else
#  define DEBUG_PRINT5(s)
# endif
#endif

#ifndef DEBUG_PRINT2
# if KDEBUG > 1
#  define DEBUG_PRINT2(s)	PRINTK s
# else
#  define DEBUG_PRINT2(s)
# endif
#endif

#ifndef DEBUG_PRINT9
# if KDEBUG > 8
#  define DEBUG_PRINT9(s)	PRINTK s
# else
#  define DEBUG_PRINT9(s)
# endif
#endif

#undef DPRINT1
#undef DPRINT2
#undef DPRINT5
#undef DPRINT9
#undef DPRINTC
#undef DPRINTC2

#define DPRINT1(s)		DEBUG_PRINT1(s)
#define DPRINT2(s)		DEBUG_PRINT2(s)
#define DPRINT5(s)		DEBUG_PRINT5(s)
#define DPRINT9(s)		DEBUG_PRINT9(s)
#define DPRINTC(s)		DEBUG_CALLS_PRINT(s)
#define DPRINTC2(args...)	DEBUG_CALLS_PRINT2(args)
#define DPRINTR2(args...)	DEBUG_RETS_PRINT2(args)


/** Memory allocation **/

#undef MALLOC
#undef FREE
#if VERIFY_MALLOC
# define START_ALLOC(m)	ovlfs_start_alloc(m)
# define STOP_ALLOC(m)	ovlfs_stop_alloc(m)
# define MALLOC(s)	ovlfs_alloc(s)
# if VERIFY_MALLOC > 1
#  define FREE(p)	ovlfs_free2((p), sizeof((p)[0]))
# else
#  define FREE(p)	ovlfs_free(p)
# endif
# define Free		ovlfs_free
#else
# define START_ALLOC(m)
# define STOP_ALLOC(m)
# define MALLOC(s)	kmalloc((s), GFP_KERNEL)
# define FREE(p)	kfree(p)
# define Free		kfree
#endif



/******************************************************************************
 ******************************************************************************
 **
 ** INLINE FUNCTIONS
 **
 ** NOTE:
 **	I don't really like header file functions, but this is the easiest way
 **	to inline this code and make it available across source files.
 **/

/**
 ** FUNCTION: ovlfs_chown
 **
 **  PURPOSE: Change the ownership of the given inode to the specified user
 **           id and group id.  If the user id or the group id is -1, then that
 **           attribute is unchanged.
 **
 ** NOTES:
 **	- Any filesystem whose super block does not implement write_inode is
 **	  likely to fail this update.
 **
 **	- There is no permission checking here; the main intent of this
 **	  function is to allow the ovlfs to set the ownership of files and
 **	  directories it creates to the ownership of the matching file or
 **	  directory in the base filesystem.
 **
 ** RETURNS:
 **	0	- If both id arguments are -1.
 **	1	- If successful.
 **/

static inline int	ovlfs_chown(struct inode *inode, uid_t uid, gid_t gid)
{
	int	ret;

	if ( uid != (uid_t) -1 )
		inode->i_uid = uid;
	else
		if ( gid == (gid_t) -1 )
			return	0;

	ret = 1;

	if ( gid != (gid_t) -1 )
		inode->i_gid = gid;

	if ( ( inode->i_sb != NULL ) &&
	     ( inode->i_sb->s_op != NULL ) &&
	     ( inode->i_sb->s_op->write_inode != NULL ) )
	{
		INODE_MARK_DIRTY(inode);

		ret = ovlfs_lock_inode(inode);

		if ( ret >= 0 )
		{
#if POST_22_KERNEL_F
			inode->i_sb->s_op->write_inode(inode, 0);
#else
			inode->i_sb->s_op->write_inode(inode);
#endif

			ovlfs_unlock_inode(inode);
		}
	}

	return	ret;
}


#endif
