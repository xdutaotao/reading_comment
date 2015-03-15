/******************************************************************************
 ******************************************************************************
 **
 ** COPYRIGHT (C) 1998-2003 By Arthur Naseef
 **
 ** This file is covered under the GNU General Public License Version 2.  For
 **  more information, see the file COPYING.
 **
 **
 ** FILE: ovl_main.c
 **
 ** DESCRIPTION: This file conatins the source code for the super block
 **              operations for the overlay filesystem.  In addition, if the
 **              filesystem is compiled as a module, this file contains the
 **              module handling code.
 **
 ** NOTES:
 **	- As of version 1.1, the ovlfs_read_inode function's use of the
 **	  ovlfs_prepare_inode function is not clean because the read inode
 **	  storage operation needs the ovlfs information to be set for the
 **	  inode, and the ovlfs_prepare_inode function is intended to set it.
 **	  The problem, however, is that ovlfs_prepare_inode needs to have the
 **	  mode of the inode already set, which is not done until
 **	  ovlfs_read_inode is called.
 **
 **	  The workaround was to perform the read first after assigning the
 **	  inode an empty ovlfs information structure and modify
 **	  ovlfs_prepare_inode to properly copy and cleanup the information
 **	  from the original copy.
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 09/27/1997	art            	Added to source code control.
 ** 02/21/1998	ARTHUR N.	Fixed ovl_fs_read_super() when the root inode
 **				 fails to be retrieved.
 ** 02/24/1998	ARTHUR N.	Added the ovlfs_write_super() method in order
 **				 to support sync'ing of the filesystem's
 **				 information.  Also, added support for hiding
 **				 or showing of MAGIC directories and added
 **				 several comments.
 ** 02/26/1998	ARTHUR N.	Added four more inode fields to the list of
 **				 fields retained in our storage.
 ** 02/27/1998	ARTHUR N.	Added the ovlfs_notify_change() operation and
 **				 moved the superblock operations structure
 **				 to the top of the file.
 ** 02/28/1998	ARTHUR N.	Make the statfs() operation somewhat
 **				 meaningful; approximate used memory.
 ** 02/28/1998	ARTHUR N.	Set superblock to be dirty on the write_inode()
 **				 operation.
 ** 02/28/1998	ARTHUR N.	After writing the superblock, mark it as clean.
 ** 03/01/1998	ARTHUR N.	Added the maxmem option and the magic directory
 **				 name options.
 ** 03/01/1998	ARTHUR N.	Allocate GFP_KERNEL priority memory only.
 ** 03/02/1998	ARTHUR N.	Keep the inode of the storage file.
 ** 03/02/1998	ARTHUR N.	Fixed bug in ovlfs_read_super() that prevented
 **				 a failed ovlfs_stg_read_super() call from
 **				 cleaning up properly.
 ** 03/03/1998	ARTHUR N.	Move common source for cleaning up the super
 **				 block's generic information to a single func.
 **				 Also, complete the fix for the
 **				 ovlfs_read_super() problem mentioned above.
 ** 03/03/1998	ARTHUR N.	Added support for the noxmnt option.
 ** 03/03/1998	ARTHUR N.	Use default file operations for non-directory,
 **				 non-regular file inodes.
 ** 03/05/1998	ARTHUR N.	Added support for the mapping storage options,
 **				 and fixed problem with root inode not being
 **				 updated properly in the storage file.
 ** 03/05/1998	ARTHUR N.	More error messages for error conditions.
 ** 03/05/1998	ARTHUR N.	Malloc-checking support added.
 ** 03/06/1998	ARTHUR N.	Free the memory used by the name of the
 **				  storage file.  Also, added code to check the
 **				  use of memory.
 ** 03/09/1998	ARTHUR N.	Added the copyright notice.
 ** 03/10/1998	ARTHUR N.	Improved comments and changed printk() calls
 **				  to use the DEBUG_PRINT macros.
 ** 03/10/1998	ARTHUR N.	Set the attributes of the root inode on mount.
 ** 03/10/1998	ARTHUR N.	Mark inode as clean after writing it.
 ** 03/10/1998	ARTHUR N.	Don't set the superblock as dirty if told so
 **				 by mount options.
 ** 03/10/1998	ARTHUR N.	Added shortcuts for mount options.
 ** 03/11/1998	ARTHUR N.	Added warning for invalid mount options.
 ** 03/28/1998	ARTHUR N.	Added use of the storage ops.
 ** 06/27/1998	ARTHUR N.	Reworked ovlfs_put_inode slightly so that the
 **				 inode is written before releasing its ovlfs
 **				 information structure, if it needs to be
 **				 written.
 ** 08/23/1998	ARTHUR N.	Reworked prepare_inode, stopped the update of
 **				 directories in read_inode(), and added loop
 **				 on inode update in put_inode().
 ** 01/16/1999	ARTHUR N.	Added registration of storage methods.
 ** 01/17/1999	ARTHUR N.	Corrected bug in use of strchr(): make sure
 **				 the return value is not NULL before de-
 **				 referencing it.
 ** 02/02/1999	ARTHUR N.	Completed implementation of the storage method
 **				 interface.
 ** 02/06/1999	ARTHUR N.	Reworked Storage System interface so that
 **				 ovlfs_read_inode() can once again call
 **				 ovlfs_read_directory().
 ** 02/07/1999	ARTHUR N.	Update directory link counts using the num
 **				 dirent storage method.
 ** 02/15/2003	ARTHUR N.	Pre-release of 2.4 port.
 ** 02/24/2003	ARTHUR N.	Created ovlfs_clear_inode_refs() and removed
 **				 ovlfs_dir_dent from the inode info structure.
 ** 02/27/2003	ARTHUR N.	Corrected cleanup after path_walk failure.
 ** 02/27/2003	ARTHUR N.	Modified set_root_inode_attr() so it will use
 **				 the base root properly and will default the
 ** 03/09/2003	ARTHUR N.	Use make_bad_inode().
 ** 03/10/2003	ARTHUR N.	Added missing label to read_inode.
 ** 03/11/2003	ARTHUR N.	Added module license and other goodies.
 ** 03/13/2003	ARTHUR N.	Force i_blksize to PAGE_SIZE when 0.
 ** 03/13/2003	ARTHUR N.	Calculate i_blocks when i_blksize and i_blocks
 **				 are both 0.
 ** 03/13/2003	ARTHUR N.	Eliminate "inodes in use" problem for storage
 **				 filesystem by correcting ovlfs_clear_inode's
 **				 setting u.generic_ip to NULL.
 ** 06/09/2003	ARTHUR N.	Corrected module version precompiler tests.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ident "@(#)ovl_main.c	1.49	[03/07/27 22:20:35]"

#include "kernel_vers_spec.h"

#ifdef MODVERSIONS
# include <linux/modversions.h>
# ifndef __GENKSYMS__
#  include "ovlfs.ver"
# endif
#endif

#ifdef MODULE
# if (! defined(__GENKSYMS__)) || (! defined(MODVERSIONS))
#  include <linux/module.h>
# endif
#endif

#include <linux/version.h>

#include <linux/stddef.h>

#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <linux/vfs.h>
#include <asm/statfs.h>
#include <asm/segment.h>

#include "ovl_debug.h"
#include "ovl_fs.h"
#include "ovl_stg.h"
#include "ovl_inlines.h"	/* Look here for do_*_op() and the like */

#if POST_20_KERNEL_F
# include <asm/uaccess.h>
#endif


/**
 ** SUPER BLOCK OPERATIONS:
 **/

static void	ovlfs_read_inode(struct inode *);
static void	ovlfs_put_inode(struct inode *);
static void	ovlfs_put_super(struct super_block *);
static void	ovlfs_write_super(struct super_block *);

#if POST_22_KERNEL_F
static void	ovlfs_write_inode(struct inode *, int);
static int	ovlfs_statfs(struct super_block *, struct statfs *);
static void	ovlfs_clear_inode(struct inode *);
#elif POST_20_KERNEL_F
static void	ovlfs_write_inode(struct inode *);
static int	ovlfs_statfs(struct super_block *, struct statfs *, int);
static void	ovlfs_clear_inode(struct inode *);
#else
static int	ovlfs_notify_change(struct inode *, struct iattr *);
static void	ovlfs_write_inode(struct inode *);
static void	ovlfs_statfs(struct super_block *, struct statfs *, int);
#endif


static struct super_operations	ovl_super_ops = {
	read_inode:	ovlfs_read_inode,
#if PRE_22_KERNEL_F
	notify_change:	ovlfs_notify_change,
#endif
	write_inode:	ovlfs_write_inode,
	put_inode:	ovlfs_put_inode,
	put_super:	ovlfs_put_super,
	write_super:	ovlfs_write_super,
	statfs:		ovlfs_statfs,
	remount_fs:	NULL,
		/* remount - Don't need it */
#if POST_20_KERNEL_F
	clear_inode:	ovlfs_clear_inode,
	umount_begin:	NULL,
#endif
#if POST_22_KERNEL_F
	fh_to_dentry:	NULL,
	dentry_to_fh:	NULL,
#endif
};


/**
 ** STORAGE OPERATIONS
 **/

/** The head is always the original "Storage File" method **/

ovlfs_storage_sys_t	StorageMethods =
	{
		"Storage File",
		0,
		&ovlfs_stg1_ops,
		0,
		NULL
	} ;



			/**************************/
			/** STORAGE METHOD ADMIN **/
			/**************************/


/**
 ** FUNCTION: lookup_storage_method
 **
 **  PURPOSE: Lookup the storage method with the specified name.
 **/

static int	lookup_storage_method (const char *name, int *id,
		                       ovlfs_storage_op_t **result)
{
	ovlfs_storage_sys_t	*ptr;
	int			ret_code;
	int			found;

		/* Verify the argument given */

	if ( ( name == (char *) NULL ) || ( id == (int *) NULL ) ||
	     ( result == (ovlfs_storage_op_t **) NULL ) )
	{
		return	-EINVAL;
	}

		/* Find the element with the specified name */

	ret_code = 0;
	ptr = &StorageMethods;
	found = FALSE;

	while ( ( ptr != (ovlfs_storage_sys_t *) NULL ) && ( ! found ) )
	{
		if ( strcmp(ptr->name, name) == 0 )
		{
			found = TRUE;
		}
		else
			ptr = ptr->next;
	}

	if ( found )
	{
		ptr->use_count++;
		id[0] = ptr->id;
		result[0] = ptr->ops;
	}
	else
		ret_code = -ENOENT;

	return	ret_code;
}



/**
 ** FUNCTION: unuse_storage_method
 **
 **  PURPOSE: Find the storage method specified and decrement its use count.
 **/

static int	unuse_storage_method (int id)
{
	ovlfs_storage_sys_t	*ptr;
	int			found;
	int			ret_code;

		/* Find the element with the specified name */

	ret_code = 0;

	ptr = &StorageMethods;
	found = FALSE;

	while ( ( ptr != (ovlfs_storage_sys_t *) NULL ) && ( ! found ) )
	{
		if ( ptr->id == id )
		{
			found = TRUE;
		}
		else
			ptr = ptr->next;
	}

	if ( found )
	{
		if ( ptr->use_count > 0 )
			ptr->use_count--;
		else
			printk("OVLFS: attempt to release storage method "
			       "with use count of 0!\n");
	}
	else
		ret_code = -1;

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_register_storage
 **
 **  PURPOSE: Register the given storage method operations with the specified
 **           name.
 **/

int	ovlfs_register_storage (const char *name, ovlfs_storage_op_t *ops)
{
	ovlfs_storage_sys_t	*new_stg;
	ovlfs_storage_sys_t	*ptr;
	ovlfs_storage_sys_t	*last;
	static int		next_id = 1;
	int			ret_code;

	if ( ( name == (char *) NULL ) ||
	     ( ops == (ovlfs_storage_op_t *) NULL ) )
	{
		return	-EINVAL;
	}

	ret_code = 0;

		/* Make sure the name is not already in use */

	last = &StorageMethods;

	for ( ptr = &StorageMethods; ptr != NULL; ptr = ptr->next )
	{
		if ( strcmp(ptr->name, name) == 0 )
		{
			printk("OVLFS: storage method, %s, already "
			       "registered.\n", name);

			return	-EEXIST;
		}

		last = ptr;
	}

		/* Allocate memory for the storage method list node */

	new_stg = (ovlfs_storage_sys_t *) MALLOC(sizeof(ovlfs_storage_sys_t));

	if ( new_stg == (ovlfs_storage_sys_t *) NULL )
	{
		printk("OVLFS: unable to allocate %d bytes to register "
		       "a storage method\n", sizeof(ovlfs_storage_sys_t));

		ret_code = -ENOMEM;
	}
	else
	{
			/* Copy the name since we can't keep the one given */

		new_stg->name = dup_string(name);

		if ( new_stg->name == (char *) NULL )
		{
			printk("OVLFS: unable to allocate %d bytes to "
			       "register a storage method\n", strlen(name));

			FREE(new_stg);
			ret_code = -ENOMEM;
		}
		else
		{
			new_stg->ops = ops;
			new_stg->use_count = 0;
			new_stg->id = next_id;
			next_id++;

				/* Now, add this storage method to the list */

			new_stg->next = (ovlfs_storage_sys_t *) NULL;
			last->next = new_stg;
		}
	}

	return	ret_code;
}



/**
 ** FUNCTION: ovlfs_unregister_storage
 **
 **  PURPOSE: Remove the named storage method from the list of storage methods.
 **/

int	ovlfs_unregister_storage (const char *name)
{
	ovlfs_storage_sys_t	*ptr;
	ovlfs_storage_sys_t	*last;
	int			found;
	int			ret_code;

	ret_code = 0;

		/* Find the storage method with the specified name */

	last = (ovlfs_storage_sys_t *) NULL;
	found = FALSE;
	ptr = &StorageMethods;

	while ( ( ptr != NULL ) && ( ! found ) )
	{
			/* Check if this element's name matches */

		if ( strcmp(ptr->name, name) == 0 )
		{
			found = TRUE;
		}
		else
		{
			last = ptr;
			ptr = ptr->next;
		}
	}

	if ( ! found )
	{
		ret_code = -ENOENT;
	}
	else if ( ptr->use_count != 0 )
	{
		printk("OVLFS: attempt to remove storage method %s while still"
		       " in use (%d)\n", name, ptr->use_count);

		ret_code = -EBUSY;
	}
	else
	{
			/* Remove the element; never remove the first */
			/*  element, however, since it is special.    */

		if ( last == (ovlfs_storage_sys_t *) NULL )
		{
			printk("OVLFS: attempt to unregister special storage "
			       "method, %s.\n", name);

			ret_code = -EINVAL;
		}
		else
		{
				/* Remove the link to the element */

			last->next = ptr->next;

				/* Release the memory used by the element */

			FREE(ptr->name);
			FREE(ptr);
		}
	}

	return	ret_code;
}



			/*******************/
			/** OVLFS OPTIONS **/
			/*******************/

/**
 ** FUNCTION: add_unused_opt
 **
 **  PURPOSE: Given the pointer to the address of the string that contains
 **           unused options, add the specified option to the end of that
 **           string, allocating the needed memory.
 **
 ** NOTES:
 **	- This method of saving the options was chosen since the storage method
 **	  can be changed with options.  This makes it easy to ensure the
 **	  correct storage method function is called for the options without
 **	  parsing the option string an extra time.
 **/

static int	add_unused_opt (char **unused, char *opt, char *value)
{
	int	len;
	char	*tmp;
	int	ret_code;

		/* Check if the buffer was already allocated. */

	if ( unused[0] == (char *) NULL )
	{
			/* This is the first option saved; just make the */
			/*  space and copy in the option.                */

		if ( value == (char *) NULL )
		{
				/* No value; just copy the option's name. */

			unused[0] = dup_string(opt);

			if ( unused[0] != NULL )
				ret_code = 0;
			else
				ret_code = -ENOMEM;
		}
		else
		{
				/* Value was given, we need to copy the */
				/*  option=value.                       */

				/* 2 = one for equal sign and one for null */

			len = strlen(opt) + strlen(value) + 2;
			unused[0] = (char *) MALLOC(len);

			if ( unused[0] != NULL )
			{
				ret_code = 0;
				sprintf(unused[0], "%s=%s", opt, value);
			}
			else
				ret_code = -ENOMEM;
		}
	}
	else
	{
			/* This is not the first option. We need to allocate */
			/*  memory for the previous options and the new one. */

			/* Determine the lentgh. 2 => comma and null byte. */

		len = strlen(unused[0]) + strlen(opt) + 2;

		if ( value != (char *) NULL )
			len += (strlen(value) + 1);	/* 1 = equal sign */

		tmp = MALLOC(len);

		if ( tmp == (char *) NULL )
			ret_code = -ENOMEM;
		else
		{
				/* Now copy the appropriate result */

			if ( value != (char *) NULL )
				sprintf(tmp, "%s,%s=%s", unused[0], opt,
				        value);
			else
				sprintf(tmp, "%s,%s", unused[0], opt);


				/* Free the previous value and save the */
				/*  new one.                            */

			FREE(unused[0]);
			unused[0] = tmp;

			ret_code = 0;
		}
	}

	return	ret_code;
}


/**
 ** FUNCTION: ovlfs_parse_opts
 **
 **  PURPOSE: Parse the options given to the mount command.
 **/

static int	ovlfs_parse_opts (char *opt_str, ovlfs_super_info_t *opts,
		                  char **unused_opts)
{
	char	*ptr;
	char	*tmp;
	int	ret_code;

	if ( opt_str == (char *) NULL )
		return	0;

		/* Loop through the entire string of options */

	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_parse_opts(%s)\n", opt_str));

		/* Initialize the string of unused options to NULL. */

	if ( unused_opts != (char **) NULL )
		unused_opts[0] = NULL;

	ret_code = 0;

	while ( ( opt_str != (char *) NULL ) && ( opt_str[0] != '\0' ) &&
	        ( ret_code >= 0 ) )
	{
			/* Look for the end of the current option */

		ptr = strchr(opt_str, ',');

		if ( ( ptr != NULL ) && ( ptr[0] == ',' ) )
		{
			ptr[0] = '\0';
			ptr++;
		}

		DEBUG_PRINT2(("OVLFS:    one_option '%s'\n", opt_str));

			/* Translate the option: see if this is an */
			/*  assignment.                            */

		tmp = strchr(opt_str, '=');

		if ( tmp == (char *) NULL )
		{
			if ( ( strcmp(opt_str, "nostorage") == 0 ) ||
			     ( strcmp(opt_str, "nost") == 0 ) )
				opts->storage_opts &= ~OVLFS_USE_STORAGE;
			else if ( ( strcmp(opt_str, "maxmem") == 0 ) ||
			          ( strcmp(opt_str, "mm") == 0 ) )
				opts->storage_opts |= OVLFS_STG_CK_MEM;
			else if ( ( strcmp(opt_str, "xmnt") == 0 ) ||
			          ( strcmp(opt_str, "xm") == 0 ) )
				opts->storage_opts |= OVLFS_CROSS_MNT_PTS;
			else if ( ( strcmp(opt_str, "noxmnt") == 0 ) ||
			          ( strcmp(opt_str, "noxm") == 0 ) )
				opts->storage_opts &= ~OVLFS_CROSS_MNT_PTS;
			else if ( ( strcmp(opt_str, "updmntonly") == 0 ) ||
			          ( strcmp(opt_str, "um") == 0 ) )
				opts->storage_opts |= OVLFS_UPD_UMOUNT_ONLY;
			else if ( ( strcmp(opt_str, "noupdmntonly") == 0 ) ||
			          ( strcmp(opt_str, "noum") == 0 ) )
				opts->storage_opts &= ~OVLFS_UPD_UMOUNT_ONLY;
#if STORE_MAPPINGS
			else if ( ( strcmp(opt_str, "storemaps") == 0 ) ||
			          ( strcmp(opt_str, "ma") == 0 ) )
				opts->storage_opts |= OVLFS_STORE_ALL_MAPS;
			else if ( ( strcmp(opt_str, "nostoremaps") == 0 ) ||
			          ( strcmp(opt_str, "noma") == 0 ) )
				opts->storage_opts &= ~OVLFS_STORE_ALL_MAPS;
			else if ( ( strcmp(opt_str, "basemap") == 0 ) ||
			          ( strcmp(opt_str, "bm") == 0 ) )
				opts->storage_opts |= OVLFS_STORE_BASE_MAP;
			else if ( ( strcmp(opt_str, "stgmap") == 0 ) ||
			          ( strcmp(opt_str, "sm") == 0 ) )
				opts->storage_opts |= OVLFS_STORE_STG_MAP;
			else if ( ( strcmp(opt_str, "nobasemap") == 0 ) ||
			          ( strcmp(opt_str, "nobm") == 0 ) )
				opts->storage_opts &= ~OVLFS_STORE_BASE_MAP;
			else if ( ( strcmp(opt_str, "nostgmap") == 0 ) ||
			          ( strcmp(opt_str, "nosm") == 0 ) )
				opts->storage_opts &= ~OVLFS_STORE_STG_MAP;
#endif
#if USE_MAGIC
			else if ( ( strcmp(opt_str, "hidemagic") == 0 ) ||
			          ( strcmp(opt_str, "hm") == 0 ) )
				opts->magic_opts |= OVLFS_HIDE_MAGIC;
			else if ( ( strcmp(opt_str, "magic") == 0 ) ||
			          ( strcmp(opt_str, "mg") == 0 ) )
				opts->magic_opts = OVLFS_ALL_MAGIC;
			else if ( ( strcmp(opt_str, "basemagic") == 0 ) ||
			          ( strcmp(opt_str, "mb") == 0 ) )
				opts->magic_opts |= OVLFS_BASE_MAGIC;
			else if ( ( strcmp(opt_str, "ovlmagic") == 0 ) ||
			          ( strcmp(opt_str, "mo") == 0 ) )
				opts->magic_opts |= OVLFS_OVL_MAGIC;
			else if ( ( strcmp(opt_str, "nomagic") == 0 ) ||
			          ( strcmp(opt_str, "nomg") == 0 ) )
				opts->magic_opts = OVLFS_NO_MAGIC;
			else if ( ( strcmp(opt_str, "nobasemagic") == 0 ) ||
			          ( strcmp(opt_str, "nomb") == 0 ) )
				opts->magic_opts &= ~OVLFS_BASE_MAGIC;
			else if ( ( strcmp(opt_str, "noovlmagic") == 0 ) ||
			          ( strcmp(opt_str, "nomo") == 0 ) )
				opts->magic_opts &= ~OVLFS_OVL_MAGIC;
			else if ( ( strcmp(opt_str, "showmagic") == 0 ) ||
			          ( strcmp(opt_str, "ms") == 0 ) )
				opts->magic_opts &= ~OVLFS_HIDE_MAGIC;
#endif
			else
			{
					/* Unused option.  Save it for use */
					/*  by the storage method init.    */

				if ( unused_opts != (char **) NULL )
				{
					ret_code = add_unused_opt(unused_opts,
					                          opt_str,
					                          NULL);
				}
				else
					DPRINT1(("OVLFS: warning: "
					         "unrecognized mount option, "
						 "%s\n", opt_str));
			}
		}
		else
		{
			tmp[0] = '\0';
			tmp++;

				/* tmp points to the value, opt_str points */
				/*  to the name.                           */

			if ( ( strcmp(opt_str, "root" ) == 0 ) ||
			     ( strcmp(opt_str, "base_root") == 0 ) ||
			     ( strcmp(opt_str, "br") == 0 ) )
				opts->base_root = tmp;
			else if ( ( strcmp(opt_str, "storage") == 0 ) ||
			          ( strcmp(opt_str, "storage_root") == 0 ) ||
			          ( strcmp(opt_str, "sr") == 0 ) )
			{
				opts->storage_opts |= OVLFS_USE_STORAGE;
				opts->storage_root = tmp;
			}
			else if ( ( strcmp(opt_str, "stg_method") == 0 ) ||
			          ( strcmp(opt_str, "method") == 0 ) )
			{
					/* In case the user uses this option */
					/*  more than once, be sure to mark  */
					/*  the current storage method as    */
					/*  unused, unless it's the default. */

				if ( opts->stg_id != 0 )
				{
					unuse_storage_method(opts->stg_id);
					opts->stg_id = 0;
				}

					/* Now lookup the requested method. */

				ret_code = lookup_storage_method(tmp,
				             &(opts->stg_id), &(opts->stg_ops));

				if ( ret_code < 0 )
				{
					printk("OVLFS: invalid storage "
					       "method, %s, requested.\n",
					       tmp);
				}
			}
#if USE_MAGIC
			else if ( ( strcmp(opt_str, "magic") == 0 ) ||
			          ( strcmp(opt_str, "smagic") == 0 ) ||
			          ( strcmp(opt_str, "sn") == 0 ) )
			{
					/* remember the name; the length */
					/*  will be determined later.    */

				opts->Smagic_name = tmp;
			}
			else if ( ( strcmp(opt_str, "magicr") == 0 ) ||
			          ( strcmp(opt_str, "bmagic") == 0 ) ||
			          ( strcmp(opt_str, "bn") == 0 ) )
			{
					/* remember the name; the length */
					/*  will be determined later.    */

				opts->Bmagic_name = tmp;
			}
#endif
			else
			{
					/* Unused option.  Save it for use */
					/*  by the storage method init.    */

				if ( unused_opts != (char **) NULL )
				{
					ret_code = add_unused_opt(unused_opts,
					                          opt_str, tmp);
				}
				else
					DPRINT1(("OVLFS: warning: "
					         "unrecognized mount option, "
					         "%s=%s\n", opt_str, tmp));
			}
		}

			/* Move to the start of the next option, if there is */
			/*  one.                                             */

		opt_str = ptr;
	}

	return	ret_code;
}



			/*****************************/
			/** OVLFS SUPPORT FUNCTIONS **/
			/*****************************/



/**
 ** FUNCTION: get_ent
 **
 **  PURPOSE: Obtain the inode, or dentry, for the path given.
 **
 ** NOTES:
 **	- Starts from the current working directory of the current task.
 **/

#if POST_22_KERNEL_F

static inline int	get_ent (const struct super_block *my_sb,
			         const char *path, const char *title,
			         struct dentry **result)
{
	struct nameidata	nd;
	int			rc;
	int			ret_code;

		/* path_init() returns 1 on "normal" and 0 to mean "done" */
		/* path_walk() calls path_release on nd when it fails.    */

	rc = path_init(path, LOOKUP_FOLLOW | LOOKUP_POSITIVE, &nd);

        if ( rc == 0 )
		ret_code = 0;
	else
		ret_code = path_walk(path, &nd);

	if ( ret_code == 0 )
	{
			/* Keep the dentry and mark it, then release the   */
			/*  path information structure, which will release */
			/*  the dentry, and the inode in-turn.             */

		dget(nd.dentry);
		result[0] = nd.dentry;
		path_release(&nd);
	}
	else
	{
		DPRINT1(("OVLFS: unable to obtain the inode for the %s "
			 "root, %s\n", title, path));
	}

	if ( ( ret_code == 0 ) && ( result[0]->d_sb == my_sb ) )
	{
			/* This should never happen since my filesystem is */
			/*  not yet mounted!                               */

		DPRINT1(("OVLFS: %s root inode is in my own filesystem?!\n",
		         title));
		dput(result[0]);
		ret_code = -ENOENT;
	}

	return	ret_code;
}

#elif POST_20_KERNEL_F

static inline int	get_ent (const struct super_block *my_sb,
			         const char *path, const char *title,
			         struct dentry **result)
{
	struct dentry	*d_ent;
	int		ret_code;

	d_ent = open_namei(path, 0, 0);

	if ( IS_ERR(d_ent) )
	{
		ret_code = PTR_ERR(d_ent);
	}
	else
	{
		result[0] = d_ent;
		ret_code = 0;
	}

	if ( ( ret_code == 0 ) && ( result[0]->d_sb == my_sb ) )
	{
			/* This should never happen since my filesystem is */
			/*  not yet mounted!                               */

		DPRINT1(("OVLFS: %s root inode is in my own filesystem?!\n",
		         title));
		dput(result[0]);
		ret_code = -ENOENT;
	}

	return	ret_code;
}

#else
	/* Pre-2.2 version */

static inline int	get_ent (const struct super_block *my_sb,
			         const char *path, const char *title,
			         struct inode **result)
{
	int	ret;

	ret =  get_inode(path, result);

	if ( ret < 0 )
	{
		DPRINT1(("OVLFS: unable to obtain the inode for the %s root, "
		         "%s\n", title, opts->base_root));
	}
	else if ( result[0]->i_sb == my_sb )
	{
			/* This should never happen since my */
			/*  filesystem is not yet mounted!   */

		DPRINT1(("OVLFS: %s root inode is in my own filesystem?!?\n",
		         title));
		IPUT(result[0]);
		ret = -ENOENT;
	}

	return	ret;
}

#endif
/**
 ** FUNCTION: fill_in_opt_info
 **
 **  PURPOSE: Fill in any information in the options structure that has
 **           not been defined and can be determined from the other
 **           information in the structure.
 **
 ** NOTES:
 **	- Currently retrieves the root and/or storage inodes.
 **/

static void	fill_in_opt_info (struct super_block *my_sb,
		                  ovlfs_super_info_t *opts)
{
	DPRINTC((KDEBUG_CALL_PREFIX "fill_in_opt_info\n"));

#if ! FASTER_AND_MORE_DANGEROUS

	if ( opts == (ovlfs_super_info_t *) NULL )
		return;

#endif


		/* Make sure the base filesystem root is defined and obtain */
		/*  the inode, if it is not already set.                    */

	if ( opts->root_ent == NULL )
	{
		if ( opts->base_root != (char *) NULL )
		{
			get_ent(my_sb, opts->base_root, "base",
			        &(opts->root_ent));
		}
		else
			DPRINT1(("OVLFS: base root not defined!\n"));
	}


		/* Make sure the storage root is defined and obtain the */
		/*  inode, if it is not already set.                    */

	if ( opts->storage_ent == NULL )
	{
		if ( opts->storage_root != (char *) NULL )
		{
			get_ent(my_sb, opts->storage_root, "storage",
			        &(opts->storage_ent));
		}
		else
		{
#if KDEBUG
# if KDEBUG < 2
				/* If the debug level is not too high, only */
				/*  warn the user about the missing storage */
				/*  root if the storage option is set.      */

			if ( opt_use_storage_p(opts[0]) )
# endif
				printk("OVLFS: storage root not defined!\n");
#endif
		}
	}


#if USE_MAGIC
		/* Make sure the magic directory names are defined */

	if ( ( opts->Smagic_name == (char *) NULL ) ||
	     ( opts->Smagic_name[0] == '\0' ) )
		opts->Smagic_name = OVLFS_MAGIC_NAME;

	if ( ( opts->Bmagic_name == (char *) NULL ) ||
	     ( opts->Bmagic_name[0] == '\0' ) )
		opts->Bmagic_name = OVLFS_MAGICR_NAME;


		/* Determine the length of the names */

	opts->Smagic_len = strlen(opts->Smagic_name);
	opts->Bmagic_len = strlen(opts->Bmagic_name);
#endif

		/* Set the storage operations to use the Storage File  */
		/*  method's operations, if they have not yet ben set. */

	if ( opts->stg_ops == (ovlfs_storage_op_t *) NULL )
		opts->stg_ops = &ovlfs_stg1_ops;
}



/**
 ** FUNCTION: dup_ovlfs_opts
 **
 **  PURPOSE: Allocate memory for a copy of an ovlfs_opt structure and its
 **           members, and then copy the information from the given structure
 **           to this newly allocated space.
 **
 ** NOTES:
 **	- Any inodes referenced by the given structure do NOT have their
 **	  reference counts updated; if the caller does not "replace" the
 **	  original with the duplicate, the counts must be updated so that both
 **	  copies of the structure are indicated as referring to the inodes.
 **/

static ovlfs_super_info_t	*dup_ovlfs_opts (struct super_block *my_sb,
				                 ovlfs_super_info_t *opts)
{
	ovlfs_super_info_t	*result;

	DPRINTC((KDEBUG_CALL_PREFIX "dup_ovlfs_opts\n"));

	result = (ovlfs_super_info_t *) MALLOC(sizeof(ovlfs_super_info_t));

	if ( result == (ovlfs_super_info_t *) NULL )
	{
		DPRINT1(("OVLFS: unable to allocate %d bytes\n",
		         sizeof(ovlfs_super_info_t)));

		return	(ovlfs_super_info_t *) NULL;
	}

		/* Initialize all pointers to NULL and integers to 0 */

	memset(result, '\0', sizeof(ovlfs_super_info_t));

	if ( opts != (ovlfs_super_info_t *) NULL )
	{
			/* Duplicate the names of the two root directories */

		if ( opts->base_root != (char *) NULL )
		{
			result->base_root = dup_string(opts->base_root);

			if ( result->base_root == (char *) NULL )
			{
				DPRINT1(("OVLFS: dup_ovlfs_opts: unable to dup"
                                         " string %s\n", opts->base_root));

				FREE(result);
				return	(ovlfs_super_info_t *) NULL;
			}
		}
		else
			result->base_root = (char *) NULL;

		if ( opts->storage_root != (char *) NULL )
		{
			result->storage_root = dup_string(opts->storage_root);

			if ( result->storage_root == (char *) NULL )
			{
				DPRINT1(("OVLFS: dup_ovlfs_opts: unable to dup"
				         " string %s\n", result->storage_root));

				if ( result->base_root != (char *) NULL )
					FREE(result->base_root);

				FREE(result);
				return	(ovlfs_super_info_t *) NULL;
			}
		}
		else
			result->storage_root = (char *) NULL;



			/* Obtain the inodes of the two root directories */

		if ( opts->root_ent == NULL )
		{
			result->root_ent = NULL;

				/* Get the base filesystem root's inode; */
				/*  use the original string in case the  */
				/*  string copy failed.                  */

			if ( opts->base_root != (char *) NULL )
			{
				get_ent(my_sb, opts->base_root, "base",
				        &(result->root_ent));
			}
		}
		else
			result->root_ent = opts->root_ent;

		if ( opts->storage_ent == NULL )
		{
			result->storage_ent = NULL;

				/* Get the storage filesystem root's inode; */
				/*  use the original string in case the     */
				/*  string copy failed.                     */

			if ( result->storage_root != (char *) NULL )
			{
				get_ent(my_sb, opts->storage_root, "storage",
				        &(opts->storage_ent));
			}
		}
		else
			result->storage_ent = opts->storage_ent;


			/* Copy the option flags */

		result->storage_opts = opts->storage_opts;
#if USE_MAGIC
		result->magic_opts   = opts->magic_opts;

			/* It is not necessary to error these dup_string()'s */
			/*  if they fail since these names are checked later */

		if ( opts->Smagic_name != NULL )
		{
			result->Smagic_name = dup_string(opts->Smagic_name);
			result->Smagic_len = opts->Smagic_len;
		}

		if ( opts->Bmagic_name != NULL )
		{
			result->Bmagic_name = dup_string(opts->Bmagic_name);
			result->Bmagic_len = opts->Bmagic_len;
		}
#endif

		DPRINT5(("OVLFS: root inode at 0x%x\n",
		         (int)result->root_ent));
		DPRINT5(("OVLFS: storage inode at 0x%x\n",
		         (int) result->storage_ent));


			/* Set the storage operations structure */

		result->stg_ops = opts->stg_ops;
	}
	else
	{
#if USE_MAGIC
		result->magic_opts = OVLFS_ALL_MAGIC;
#endif
	}

	return	result;
}



/**
 ** FUNCTION: free_super_info
 **
 **  PURPOSE: Free the memory allocated to the given option structure and its
 **           members.
 **
 ** NOTES:
 **	- The Release() storage operation must be performed before this
 **	  function is called, as necessary.
 **/

static void	free_super_info (ovlfs_super_info_t *sup_info)
{
	DPRINTC((KDEBUG_CALL_PREFIX "free_super_info\n"));

	if ( sup_info != (ovlfs_super_info_t *) NULL )
	{
		if ( sup_info->base_root != (char *) NULL )
			FREE(sup_info->base_root);

		if ( sup_info->storage_root != (char *) NULL )
			FREE(sup_info->storage_root);

#if USE_MAGIC
		if ( sup_info->Smagic_name != (char *) NULL )
			FREE(sup_info->Smagic_name);

		if ( sup_info->Bmagic_name != (char *) NULL )
			FREE(sup_info->Bmagic_name);
#endif

#if ! FASTER_AND_MORE_DANGEROUS
			/* Zero out the structure for safety's sake */

		memset(sup_info, '\0', sizeof(ovlfs_super_info_t));
#endif

		FREE(sup_info);
	}
}




/**
 ** FUNCTION: dup_inode_info
 **
 **  PURPOSE: Duplicate the inode information structure given.  The result
 **           must be freed with free_inode_info().
 **/

static ovlfs_inode_info_t	*dup_inode_info (ovlfs_inode_info_t *i_info)
{
	ovlfs_inode_info_t	*result;

	DPRINTC((KDEBUG_CALL_PREFIX "dup_inode_info\n"));

#if ! FASTER_AND_MORE_DANGEROUS

	if ( i_info == (ovlfs_inode_info_t *) NULL )
		return	(ovlfs_inode_info_t *) NULL;

#endif

	result = (ovlfs_inode_info_t *) MALLOC(sizeof(ovlfs_inode_info_t));

	if ( result != (ovlfs_inode_info_t *) NULL )
	{
		if ( ( i_info->s.name != NULL ) && ( i_info->s.len > 0 ) )
		{
			result->s.name = dup_buffer(i_info->s.name,
			                            i_info->s.len);

			if ( result->s.name == (char *) NULL )
			{
				FREE(result);
				return	(ovlfs_inode_info_t *) NULL;
			}

			result->s.len = i_info->s.len;
		}
		else
		{
			result->s.name = (char *) NULL;
			result->s.len = 0;
		}

#if STORE_REF_INODES
# if REF_DENTRY_F
		result->base_dent      = i_info->base_dent;
		result->overlay_dent   = i_info->overlay_dent;
# else
		result->ovlfs_dir     = i_info->ovlfs_dir;
		result->base_inode    = i_info->base_inode;
		result->overlay_inode = i_info->overlay_inode;
# endif
#else
		if ( i_info->base_name != (char *) NULL )
		{
			result->base_name = dup_string(i_info->base_name);

			if ( result->base_name == (char *) NULL )
			{
				if ( result->s.name != (char *) NULL )
				{
					FREE(result->s.name);
					result->s.name = (char *) NULL;
				}

				FREE(result);
				return	(ovlfs_inode_info_t *) NULL;
			}
		}
		else
			result->base_name = (char *) NULL;

		if ( i_info->overlay_name != (char *) NULL )
		{
			result->overlay_name = dup_string(i_info->overlay_name);

			if ( result->overlay_name == (char *) NULL )
			{
				if ( result->s.name != (char *) NULL )
				{
					FREE(result->s.name);
					result->s.name = (char *) NULL;
				}

				if ( result->base_name != (char *) NULL )
					FREE(result->base_name);

				FREE(result);
				return	(ovlfs_inode_info_t *) NULL;
			}
		}
		else
			result->overlay_name = (char *) NULL;
#endif

#if POST_20_KERNEL_F
		sema_init(&(result->page_io_file_sem), 1);
		result->page_file_which = 0;
		result->page_io_file    = NULL;
#endif
	}

	return	result;
}



/**
 ** FUNCTION: free_inode_info
 **
 **  PURPOSE: Free the kernel memory allocated to the given inode information
 **           structure.
 **/

static void	free_inode_info (ovlfs_inode_info_t *i_info)
{
	DPRINTC((KDEBUG_CALL_PREFIX "free_inode_info\n"));

#if ! FASTER_AND_MORE_DANGEROUS

	if ( i_info == (ovlfs_inode_info_t *) NULL )
	{
		DPRINT1(("OVLFS: free_inode_info called with NULL pointer\n"));
		return;
	}

#endif

#if POST_20_KERNEL_F
	release_page_io_file(i_info);
	i_info->page_file_which = 0;
	i_info->page_io_file    = NULL;
	sema_init(&(i_info->page_io_file_sem), 0);
#endif

	if ( ( i_info->s.name != (char *) NULL ) && ( i_info->s.len > 0 ) )
	{
		FREE(i_info->s.name);
		i_info->s.name = (char *) NULL;
	}

#if ! STORE_REF_INODES
	if ( i_info->base_name != (char *) NULL )
	{
		FREE(i_info->base_name);
		i_info->base_name = (char *) NULL;
	}

	if ( i_info->overlay_name != (char *) NULL )
	{
		FREE(i_info->overlay_name);
		i_info->overlay_name = (char *) NULL;
	}
#endif

#if ! FASTER_AND_MORE_DANGEROUS
			/* Zero out the structure for safety's sake */

	memset(i_info, '\0', sizeof(ovlfs_inode_info_t));
#endif

	FREE(i_info);
}



		/**********************************/
		/** OVLFS SUPER BLOCK OPERATIONS **/
		/**********************************/


/**
 ** FUNCTION: update_dir_link_count
 **
 **  PURPOSE: Given the inode for a directory in the psuedo filesystem, update
 **           the link count of the directory.
 **
 ** NOTES:
 **	- If the storage method, Num_dirent(), fails, the default value given
 **	  is used as the number of entries in the directory.
 **/

static inline void	update_dir_link_count (struct inode *dir, int def_cnt)
{
	int	num_ent;

	num_ent = do_num_dirent_op(dir, FALSE);

	if ( num_ent < 0 )
		num_ent = def_cnt;

#if USE_MAGIC
		/* Include the magic directories as well as . and the entry */
		/*  in the parent directory.                                */

	num_ent += 4;
#else
		/* Include . and link in parent directory */

	num_ent += 2;
#endif

	dir->i_nlink = num_ent;
}



/**
 ** FUNCTION: ovlfs_prepare_inode
 **
 **  PURPOSE: Prepare the inode for use in the overlay filesystem.  This
 **           function prepares the ovlfs inode information structure for
 **           the inode.
 **/

static void	ovlfs_prepare_inode (struct inode *rd_ino, int is_root_f)
{
	ovlfs_inode_info_t	inode_info;
	ovlfs_inode_info_t	*tmp;

	DPRINTC2("i_ino %ld, is_root_f %d\n", rd_ino->i_ino, is_root_f);

		/* Set the entire structure to 0's (pointers to NULL), or */
		/*  copy the existing information, if it is there.        */

	if ( rd_ino->u.generic_ip == NULL )
		memset(&inode_info, '\0', sizeof(ovlfs_inode_info_t));
	else
		memcpy(&inode_info, rd_ino->u.generic_ip, sizeof(inode_info));

#if ! STORE_REF_INODES
	if ( is_root_f )
	{
		ovlfs_super_info_t	*opts;

			/* Default the root inode's mode. */

		root_ino->mode |= S_IFDIR | S_IRUGO | S_IXUGO;

		if ( ( rd_ino->i_sb == (struct super_block *) NULL ) ||
		     ( rd_ino->i_sb->u.generic_sbp == NULL ) )
		{
			BUG();
		}

		opts = (ovlfs_super_info_t *) rd_ino->i_sb->u.generic_sbp;

		inode_info.base_name    = opts->base_root;
		inode_info.overlay_name = opts->storage_root;
	}
#endif


		/* If the inode already has inode info, release it after */
		/*  making the copy.  It must be done afterwards because */
		/*  inode_info may have references to the same strings.  */

	if ( rd_ino->u.generic_ip != NULL )
	{
			/* Remember the old so it can be released later. */
		tmp = (ovlfs_inode_info_t *) rd_ino->u.generic_ip;

			/* Copy the new one. */
		rd_ino->u.generic_ip = (void *) dup_inode_info(&inode_info);

			/* Release the old one now. */
		free_inode_info(tmp);
	}
	else
	{
			/* Just copy it in there. */
		rd_ino->u.generic_ip = (void *) dup_inode_info(&inode_info);
	}

	DPRINTR2("i_ino %ld, is_root_f %d\n", rd_ino->i_ino, is_root_f);
}


#if PRE_24_KERNEL_F

/**
 ** FUNCTION: init_special_inode
 **/

void	init_special_inode (struct inode *inode, umode_t mode, int rdev)
{
        if (S_ISCHR(inode->i_mode))
                inode->i_op = &chrdev_inode_operations;
        else if (S_ISBLK(inode->i_mode))
                inode->i_op = &blkdev_inode_operations;
        else if (S_ISSOCK(inode->i_mode))
                inode->i_op = NULL;
        else if (S_ISFIFO(inode->i_mode))
                init_fifo(inode);
}

#endif



/**
 ** FUNCTION: ovlfs_finalize_inode_prep
 **
 **  PURPOSE: Finalize the preparation of the inode for use by the VFS.
 **/

static void	ovlfs_finalize_inode_prep (struct inode *rd_ino)
{
	ovlfs_inode_info_t	*i_info;
	int			rc;

	DPRINTC2("rd_ino 0x%08x, i_ino %lu\n", (int) rd_ino, rd_ino->i_ino);


	i_info = (ovlfs_inode_info_t *) rd_ino->u.generic_ip;

	ASSERT(i_info != NULL);


		/* If the block size is currently 0, force it to the system */
		/*  page size.  In addition, if i_blocks is zero in this    */
		/*  case, set it to ( inode size / block size ).            */

	if ( rd_ino->i_blksize == 0 )
	{
		rd_ino->i_blksize = PAGE_SIZE;

		if ( rd_ino->i_blocks == 0 )
			rd_ino->i_blocks = ( rd_ino->i_size + 511 ) / 512;
	}

#if HAVE_I_BLKBITS
		/* Set blkbits to match the block size. */
	if ( rd_ino->i_blksize != 0 )
	{
		rd_ino->i_blkbits = ffs(rd_ino->i_blksize) - 1;
		ASSERT(rd_ino->i_blksize == ( 1 << rd_ino->i_blkbits ));
	}
	else
	{
		rd_ino->i_blkbits = 0;
	}
#endif

	if ( S_ISDIR(rd_ino->i_mode) )
	{
		if ( rd_ino->i_nlink < 2 )
			rd_ino->i_nlink = 2;

#if POST_22_KERNEL_F
		rd_ino->i_op  = &ovlfs_ino_ops;
		rd_ino->i_fop = &ovlfs_dir_fops;
#else
		rd_ino->i_op  = &ovlfs_ino_ops;
#endif

		if ( rd_ino->i_nlink < 2 )
			rd_ino->i_nlink = 2;


			/* The inode is a directory, fill in or update its */
			/*  contents now.                                  */

		rc = ovlfs_read_directory(rd_ino);

		if ( rc < 0 )
		{
			DPRINT1(("OVLFS: ovlfs_read_inode: read ovlfs "
			         "directory, %ld, returned error %d\n",
				 rd_ino->i_ino, rc));
		}
		else
		{
			update_dir_link_count(rd_ino, rc);
		}
	}
#if OVLFS_NORMAL_SPECIAL_INODE_HANDLING_F
	else if ( (S_ISCHR(rd_ino->i_mode)) || (S_ISFIFO(rd_ino->i_mode)) ||
	          (S_ISSOCK(rd_ino->i_mode)) || (S_ISBLK(rd_ino->i_mode)) )
	{
			/* Use the normal kernel handling for special files. */

		init_special_inode(rd_ino, rd_ino->i_mode, rd_ino->i_rdev);
	}
#endif
	else
	{
			/* The root inode is a directory. */

		if ( rd_ino->i_ino == OVLFS_ROOT_INO )
			BUG();

#if POST_22_KERNEL_F
		if ( S_ISLNK(rd_ino->i_mode) )
			rd_ino->i_op = &ovlfs_symlink_ino_ops;
		else
			rd_ino->i_op  = &ovlfs_ino_ops;

		rd_ino->i_fop = &ovlfs_file_ops;
#else
		rd_ino->i_op  = &ovlfs_file_ino_ops;
#endif

			/* This following would be very bad: */

		if ( rd_ino->i_mode == 0 )
			rd_ino->i_mode = S_IFREG | S_IRUGO | S_IXUGO;
	}

	if ( rd_ino->i_ino == OVLFS_ROOT_INO )
	{
		ovlfs_super_info_t	*opts;

		if ( ( rd_ino->i_sb == (struct super_block *) NULL ) ||
		     ( rd_ino->i_sb->u.generic_sbp == NULL ) )
		{
			BUG();
		}

		opts = (ovlfs_super_info_t *) rd_ino->i_sb->u.generic_sbp;

#if POST_22_KERNEL_F
		rd_ino->i_fop = &ovlfs_dir_fops;
#endif

#if STORE_REF_INODES
# if REF_DENTRY_F

			/* Get the inodes from the other fs's */

		i_info->base_dent    = opts->root_ent;
		i_info->overlay_dent = opts->storage_ent;


			/* Mark the dentries as used so that we don't */
			/*  lose them until this inode is put.        */

		if ( i_info->base_dent != DENTRY_NULL )
			dget(i_info->base_dent);

		if ( i_info->overlay_dent != DENTRY_NULL )
			dget(i_info->overlay_dent);

#  if KDEBUG > 5
		if ( i_info->base_dent != DENTRY_NULL )
			printk("OVLFS: ovlfs_finalize_inode_prep: ROOT INODE "
			       "base is inode %ld at 0x%x\n",
			       i_info->base_dent->d_inode->i_ino,
			       (int) i_info->base_dent->d_inode);
		else
			printk("OVLFS: finalize_inode_prep: ROOT INODE base "
			       "is null\n");
#  endif
# else

			/* Get the inodes from the other fs's */

		i_info->base_inode    = opts->root_ent;
		i_info->overlay_inode = opts->storage_inode;


			/* Mark the inodes as used so that we don't */
			/*  lose them until this inode is put.      */

		if ( i_info->base_inode != INODE_NULL )
			IMARK(i_info->base_inode);

		if ( i_info->overlay_inode != INODE_NULL )
			IMARK(i_info->overlay_inode);

#  if KDEBUG > 5
		if ( i_info->base_inode != INODE_NULL )
			printk("OVLFS: ovlfs_finalize_inode_prep: ROOT INODE "
			       "base is inode %ld at 0x%x\n",
			       i_info->base_inode->i_ino,
			       (int) i_info->base_inode);
		else
			printk("OVLFS: ovlfs_finalize_inode_prep: ROOT INODE "
			       "base is null\n");
#  endif
# endif
#else
		i_info->base_name    = opts->base_root;
		i_info->overlay_name = opts->storage_root;
#endif
	}
}



/**
 ** FUNCTION: set_root_inode_attr
 **
 **  PURPOSE: Given the root inode of the overlay filesystem for the first
 **           time, copy the attributes of the storage or base filesystem's
 **           root inode into the pseudo filesystem's root inode.
 **/

static void	set_root_inode_attr (struct inode *inode)
{
	struct inode		*o_inode;
	ovlfs_inode_info_t	*i_info;
	nlink_t			cnt;
	int			ret;
	int			att_set_f;

	DPRINTC2("inode at 0x%08x\n", (int) inode);

	i_info = (ovlfs_inode_info_t *) inode->u.generic_ip;

	if ( i_info == (ovlfs_inode_info_t *) NULL )
	{
		DPRINT1(("OVLFS: set_root_inode_attr: inode info is NULL!\n"));
		goto	set_root_inode_attr__out;
	}

		/* Update the storage inode reference, if needed. */

	att_set_f = FALSE;

	if ( i_info->s.stg_dev == NODEV )
	{
		ret = ovlfs_resolve_ovl_inode(inode, &o_inode);

		if ( ret >= 0 )
		{
			i_info->s.stg_dev = o_inode->i_dev;
			i_info->s.stg_ino = o_inode->i_ino;

			cnt = inode->i_nlink;	/* maintain link cnt */
			copy_inode_info(inode, o_inode);
			att_set_f = TRUE;

			inode->i_nlink = cnt;

			IPUT(o_inode);
		}
	}

		/* Update the base inode reference if needed. */

	if ( i_info->s.base_dev == NODEV )
	{
		ret = ovlfs_resolve_base_inode(inode, &o_inode);

		if ( ret >= 0 )
		{
			i_info->s.base_dev = o_inode->i_dev;
			i_info->s.base_ino = o_inode->i_ino;

				/* maintain the link count */
			cnt = inode->i_nlink;

				/* Copy the inode attributes, if needed. */

			if ( ! att_set_f )
			{
				copy_inode_info(inode, o_inode);
				att_set_f = TRUE;
			}
			inode->i_nlink = cnt;

			IPUT(o_inode);
		}
	}

		/* If no attributes were set, and the inode's mode is */
		/*  invalid, give it minimally valid settings now.    */
		/*  This should never happen in the normal course of  */
		/*  operation, as the mount should fail if both the   */
		/*  base and storage roots are missing.               */

	if ( ( ! att_set_f ) && ( inode->i_mode == 0 ) )
		inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;

set_root_inode_attr__out:

	DPRINTR2("inode at 0x%08x\n", (int) inode);
}



/**
 ** FUNCTION: do_inode_att_update
 **
 **  PURPOSE: Update the attibutes of the given ovlfs inode based on its
 **           storage or base fs version.
 **
 ** RULES:
 **	- Must only be called on an inode which must be updated.
 **/

static int	do_inode_att_update (struct inode *inode)
{
	struct inode	*o_inode;
	int		ret;

		/* If an inode exists in the storage filesystem, use */
		/*  it; otherwise, use the base inode.               */

	ret = ovlfs_resolve_ovl_inode(inode, &o_inode);

	if ( ret < 0 )
		ret = ovlfs_resolve_base_inode(inode, &o_inode);

	if ( ret >= 0 )
	{
			/* Do not worry about the link count for dirs, it */
			/*  will be updated after each dir's contents are */
			/*  updated.                                      */

		copy_inode_info(inode, o_inode);
		inode->i_rdev = o_inode->i_rdev;

		INODE_MARK_DIRTY(inode);

		IPUT(o_inode);
	}
	else
	{
		DPRINT2(("OVLFS: do_inode_att_update: can't get base or "
			 "storage fs inode for ovlfs inode %lu\n",
		         inode->i_ino));
	}

	return	ret;
}



/**
 ** FUNCTION: ovlfs_read_inode
 **
 **  PURPOSE: Implement the read_inode super-block operation.
 **/

static void	ovlfs_read_inode (struct inode *rd_ino)
{
	int	ret;
	int	valid_f;

	DPRINTC2("ino %ld at 0x%x\n", rd_ino->i_ino, (int) rd_ino);

		/* Initialize the inode's attributes */

	rd_ino->i_op      = &ovlfs_ino_ops;
	rd_ino->i_mode    = 0;
	rd_ino->i_uid     = 0;
	rd_ino->i_gid     = 0;
	rd_ino->i_nlink   = 1;
	rd_ino->i_size    = 0;
	rd_ino->i_mtime   = CURRENT_TIME;
	rd_ino->i_atime   = CURRENT_TIME;
	rd_ino->i_ctime   = CURRENT_TIME;
	rd_ino->i_blocks  = 0;
	rd_ino->i_blksize = PAGE_SIZE;
#if HAVE_I_BLKBITS
	rd_ino->i_blkbits = PAGE_SHIFT;
#endif
	rd_ino->u.generic_ip     = NULL;

#if POST_22_KERNEL_F
	rd_ino->i_mapping->a_ops = &ovlfs_addr_ops;
#endif

		/* The root inode is a special case */

	if ( rd_ino->i_ino == OVLFS_ROOT_INO )
	{
			/* Fill in the inode's information structure */

		ovlfs_prepare_inode(rd_ino, TRUE);


			/* Read the directory information; the number of   */
			/*  entries in the directory is returned if        */
			/*  successful; set nlink to 2 + ret, unless magic */
			/*  is compiled in, in which case, use 4 + ret.    */

		ret = do_read_inode_op(rd_ino, &valid_f);


			/* If that failed, make the inode bad and get out. */
			/*  Note that the usual cleanup of the inode will  */
			/*  occur later, "as usual".                       */

		if ( ret < 0 )
		{
			make_bad_inode(rd_ino);
			goto	ovlfs_read_inode__out;
		}


			/* Set the attributes for the root.  Don't worry    */
			/*  about updating the storage information; it will */
			/*  be updated when the inode is written.           */

		set_root_inode_attr(rd_ino);

			/* Finalize it. */

		ovlfs_finalize_inode_prep(rd_ino);

			/* Mark it as dirty in case it is not already. */

		INODE_MARK_DIRTY(rd_ino);
	}
	else
	{
			/* First, read the inode's information from storage, */
			/*  but, before that, allocate an ovlfs inode        */
			/*  information structure so that the read operation */
			/*  fill it in.                                      */

		ovlfs_prepare_inode(rd_ino, FALSE);

		ret = do_read_inode_op(rd_ino, &valid_f);

		if ( ret < 0 )
		{
			make_bad_inode(rd_ino);
			DPRINT1(("OVLFS: ovlfs_read_inode: do_read_inode_op "
			         "returned error %d\n", ret));
		}
		else
		{
				/* Check if the inode needs to be updated */
				/*  and update it now if so.              */

			if ( ! valid_f )
				ret = do_inode_att_update(rd_ino);

			ovlfs_finalize_inode_prep(rd_ino);
		}
	}

ovlfs_read_inode__out:
	DPRINTR2("ino %ld at 0x%x\n", rd_ino->i_ino, (int) rd_ino);
}



#if PRE_22_KERNEL_F

/**
 ** FUNCTION: ovlfs_notify_change
 **
 **  PURPOSE: This if the overlay filesystem's notify_change() superblock
 **           operation.
 **/

static int	ovlfs_notify_change (struct inode *inode, struct iattr *attr)
{
	struct inode	*o_inode;
	int		ret;

	if ( ! is_ovlfs_inode(inode) )
		return	-ENOENT;

		/* If an storage inode exists, update it; otherwise, just */
		/*  update our own inode's information.                   */

	ret = ovlfs_resolve_ovl_inode(inode, &o_inode);

	if ( ret >= 0 )
	{
		if ( ( o_inode->i_sb != NULL ) &&
		     ( o_inode->i_sb->s_op != NULL ) &&
		     ( o_inode->i_sb->s_op->notify_change != NULL ) )
		{
			DOWN(&(o_inode->i_sem));
			ret = o_inode->i_sb->s_op->notify_change(o_inode, attr);
			UP(&(o_inode->i_sem));
		}
		else
		{
			ret = inode_change_ok(o_inode, attr);

			if ( ret == 0 )
				inode_setattr(o_inode, attr);
		}

		IPUT(o_inode);
	}
	else
		ret = 0;	/* It is OK to not find the OVL inode */


		/* Now update the attributes of the psuedo-fs' inode */

	if ( ret >= 0 )
	{
		ret = inode_change_ok(inode, attr);

		if ( ret == 0 )
			inode_setattr(inode, attr);
	}

	return	ret;
}

#endif



/**
 ** FUNCTION: ovlfs_write_inode
 **
 **  PURPOSE: Implement the write_inode super-block operation.
 **
 ** NOTES:
 **	- All writes are synchronous.  This may change in the future for
 **	  kernels after 2.0.
 **/

#if POST_22_KERNEL_F
static void	ovlfs_write_inode (struct inode *inode, int sync_f)
#else
static void	ovlfs_write_inode (struct inode *inode)
#endif
{
	int	ret;

#if ! FASTER_AND_MORE_DANGEROUS

	if ( ( inode == INODE_NULL ) || ( inode->i_sb == NULL ) ||
	     ( ! is_ovlfs_inode(inode) ) )
	{
		DPRINT1(("OVLFS: ovlfs_write_inode: invalid inode given at "
		         "0x%x\n", (int) inode));

		return;
	}

#endif

	DPRINTC2("ino %ld at 0x%x\n", inode->i_ino, (int) inode);

		/* Perform the storage update operation */

	ret = do_update_inode(inode);

	if ( ret >= 0 )
	{
			/* Force the superblock to get updated before long */
			/*  unless told not to.                            */

		if ( ! ( ovlfs_sb_opt(inode->i_sb, storage_opts) &
		         OVLFS_UPD_UMOUNT_ONLY ) )
		{
			inode->i_sb->s_dirt = 1;
		}

		INODE_CLEAR_DIRTY(inode);
	}
	else
		DPRINT1(("OVLFS: write_inode: inode %d, device 0x%x: error %d "
		         "from update inode\n", (int) inode->i_ino,
		         (int) inode->i_dev, ret));

	DPRINTR2("ino %ld at 0x%x\n", inode->i_ino, (int) inode);
}



#if POST_20_KERNEL_F

/**
 ** FUNCTION: ovlfs_clear_inode_refs
 **
 **  PURPOSE: Clear the reference members of the given inode's information
 **           structure.
 **/

static void	__ovlfs_clear_inode_refs (struct inode *an_inode)
{
#if STORE_REF_INODES
# if REF_DENTRY_F
	struct dentry		*tmp_dent;
# else
	struct inode		*tmp_inode;
# endif
#endif
	ovlfs_inode_info_t	*infop;

	DPRINTC2("ino %ld\n", an_inode->i_ino);

		/* Remember the inode information. */

	infop = (ovlfs_inode_info_t *) an_inode->u.generic_ip;


		/* Now release the inode information */

	if ( infop != (ovlfs_inode_info_t *) NULL )
	{
#if STORE_REF_INODES
# if REF_DENTRY_F
		if ( infop->base_dent != DENTRY_NULL )
		{
			tmp_dent = infop->base_dent;
			infop->base_dent = NULL;
			dput(tmp_dent);
		}

		if ( infop->overlay_dent != DENTRY_NULL )
		{
			tmp_dent = infop->overlay_dent;
			infop->overlay_dent = NULL;
			dput(tmp_dent);
		}
# else
		if ( infop->ovlfs_dir != INODE_NULL )
		{
			tmp_inode = infop->ovlfs_dir;
			infop->ovlfs_dir = NULL;
			IPUT(tmp_inode);
		}

		if ( infop->base_inode != INODE_NULL )
		{
			tmp_inode = infop->base_inode;
			infop->base_inode = NULL;
			IPUT(tmp_inode);
		}

		if ( infop->overlay_inode != INODE_NULL )
		{
			tmp_inode = infop->overlay_inode;
			infop->overlay_inode = NULL;
			IPUT(tmp_inode);
		}
# endif
#endif
	}

	DPRINTR2("ino %ld\n", an_inode->i_ino);
}

void	ovlfs_clear_inode_refs (struct inode *an_inode)
{
	__ovlfs_clear_inode_refs(an_inode);
}



/**
 ** FUNCTION: ovlfs_clear_inode
 **
 **  PURPOSE: Perform the fs clear_inode operation for an ovlfs super block.
 **           The main purpose of the clear_inode operation is to clean up
 **           the inode just before returning it to the "inode free pool".
 **/

static void	ovlfs_clear_inode (struct inode *an_inode)
{
	ovlfs_inode_info_t	*infop;

	DPRINTC2("ino %ld\n", an_inode->i_ino);

		/* Remember the inode information. */

	infop = (ovlfs_inode_info_t *) an_inode->u.generic_ip;

		/* Now release the inode information */

	if ( infop != (ovlfs_inode_info_t *) NULL )
	{
			/* Clear the reference members. */

		ovlfs_clear_inode_refs(an_inode);

			/* Free the memory allocated to the inode's */
			/*  information structure.                  */

		an_inode->u.generic_ip = NULL;
		free_inode_info(infop);
	}

	DPRINTR2("ino %ld\n", an_inode->i_ino);
}

#endif



/**
 ** FUNCTION: ovlfs_put_inode
 **
 **  PURPOSE: Perform the fs put_inode operation for an ovlfs super block.
 **           The main purpose of the put_inode operation is to remove an
 **           inode/file from the filesystem when it is no longer used
 **           (i.e. when i_nlink = 0).
 **
 ** NOTES:
 **	- It is important that the inode is not reused after this function is
 **	  called without being re-read with ovlfs_read_inode.  This is due to
 **	  the fact that the overlay filesystem maintains kmalloc'ed memory in
 **	  the inode's information and there is no other place to allocate/free
 **	  this memory other than the read/put methods.
 **/

static void	ovlfs_put_inode (struct inode *an_inode)
{
#if PRE_22_KERNEL_F
	ovlfs_inode_info_t	*infop;
	int			rc = 0;
#endif

	DPRINTC2("ino %ld\n", an_inode->i_ino);

		/* If the inode is being deleted, set its size to 0 also. */

	if ( an_inode->i_nlink == 0 )
		an_inode->i_size = 0;


		/* If the inode is dirty, write it out now. */

#if POST_22_KERNEL_F

	if ( INODE_DIRTY_P(an_inode) )
		write_inode_now(an_inode, TRUE);

#elif POST_20_KERNEL_F

	if ( INODE_DIRTY_P(an_inode) )
		write_inode_now(an_inode);

#else
	while ( INODE_DIRTY_P(an_inode) )
	{
			/* Lock the inode before writing it. */

		rc = ovlfs_wait_on_inode(an_inode);

		if ( ( rc == 0 ) && ( INODE_DIRTY_P(an_inode) ) )
		{
			an_inode->i_lock = 1;

			ovlfs_write_inode(an_inode, 0);

			ovlfs_unlock_inode(an_inode);
		}
	}


		/* Kernels >= 2.2.0 now have a clear_inode() entry point - */
		/*  thank goodness!  For 2.0 kernels, keep doing the old   */
		/*  way.                                                   */

		/* Prevent the inode from being re-used out of the pool  */
		/*  without doing a read_inode().                        */

		/* Remember the inode information before clearing the inode */
		/*  so that the information can be released.                */

	infop = (ovlfs_inode_info_t *) an_inode->u.generic_ip;
	an_inode->u.generic_ip = NULL;

	clear_inode(an_inode);

		/* Now release the inode information */

	if ( infop != (ovlfs_inode_info_t *) NULL )
	{
# if STORE_REF_INODES
		if ( infop->ovlfs_dir != INODE_NULL )
			IPUT(infop->ovlfs_dir);

		if ( infop->base_inode != INODE_NULL )
			IPUT(infop->base_inode);

		if ( infop->overlay_inode != INODE_NULL )
			IPUT(infop->overlay_inode);
# endif

			/* Free the memory allocated to the inode's */
			/*  information structure.                  */

		free_inode_info(infop);
	}
#endif

	DPRINTR2("ino %ld\n", an_inode->i_ino);
}



/**
 ** FUNCTION: cleanup_super
 **
 **  PURPOSE: Cleanup the given superblock when an error occurs while
 **           reading it.
 **
 ** NOTES:
 **	- If the Storage Method's Init() operation completed successfully,
 **	  its Release() method MUST be called before this function is called.
 **
 **	- After this function is called, NO calls should be made to the storage
 **	  method's operations.
 **/

static void	cleanup_super (struct super_block *sb)
{
	ovlfs_super_info_t *s_info;

	if ( ( sb == NULL ) || ( sb->u.generic_sbp == NULL ) )
		return;

	s_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

		/* Release the storage method used by the super block */
		/*  unless it was the default.                        */

	if ( s_info->stg_id != 0 )
		unuse_storage_method(s_info->stg_id);

	if ( s_info->root_ent != NULL )
#if USE_DENTRY_F
		dput(s_info->root_ent);
#else
		IPUT(s_info->root_ent);
#endif

	if ( s_info->storage_ent != NULL )
#if USE_DENTRY_F
		dput(s_info->storage_ent);
#else
		IPUT(s_info->storage_ent);
#endif


		/* Free the memory allocated for the generic_sbp structure */

	free_super_info(s_info);
	sb->u.generic_sbp = NULL;
}



/**
 ** FUNCTION: ovlfs_put_super
 **
 **  PURPOSE: Perform the fs put_super operation for an ovlfs super block.
 **/

static void	ovlfs_put_super (struct super_block *sup_blk)
{
	DPRINTC((KDEBUG_CALL_PREFIX "ovlfs_put_super\n"));

	LOCK_SUPER20(sup_blk);


		/* Release the overlay and storage inodes, and the */
		/*  storage methods as well.                       */

	do_release_super_op(sup_blk);

	cleanup_super(sup_blk);

#if PRE_22_KERNEL_F

		/* For >= 2.2, there is nothing to be done; put_super() is  */
		/*  called just before the super block is removed from use. */

		/* Set the super block's device number to 0 to keep the  */
		/*  super block from being used again without re-reading */

	sup_blk->s_dev = 0;
#endif

	UNLOCK_SUPER20(sup_blk);

	STOP_ALLOC("ovlfs_put_super");

#if MODULE
	MOD_DEC_USE_COUNT;
#endif

	DPRINTC(("OVLFS: put super returning\n"));
}



/**
 ** FUNCTION: ovlfs_write_super
 **
 **  PURPOSE: Perform the fs write_super operation for an ovlfs superblock.
 **/

static void	ovlfs_write_super (struct super_block *sup_blk)
{
	ovlfs_super_info_t	*sup_info;

	DPRINTC2("sup_blk 0x%x\n", (int) sup_blk);

	LOCK_SUPER20(sup_blk);

	sup_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Sync == NULL ) )
	{
		DPRINT1(("OVLFS: ovlfs_write_super: storage ops or Sync method"
		         " NULL!\n"));
	}
	else
		sup_info->stg_ops->Sync(sup_blk);


	UNLOCK_SUPER20(sup_blk);

		/* Mark the super block as clean, even if an error occurred. */

	sup_blk->s_dirt = 0;

	DPRINTR2("sup_blk 0x%x\n", (int) sup_blk);
}



/**
 ** FUNCTION: ovlfs_statfs
 **
 **  PURPOSE: Perform the fs statfs operation for an ovlfs super block.
 **
 ** NOTES:
 **	- The buffer is used to allows the values to be assigned in kernel
 **	  space before using memcpy_tofs() to copy them back to user space.
 **	  The altenative is to find the address of each member of the
 **	  structure in user space and use the put_user() macro.
 **
 **	- The constantly changing interface.
 **/

#if POST_22_KERNEL_F
static int	ovlfs_statfs (struct super_block *sup_blk,
		              struct statfs *fs_info)
#elif POST_20_KERNEL_F
static int	ovlfs_statfs (struct super_block *sup_blk,
		              struct statfs *fs_info, int size)
#else
static void	ovlfs_statfs (struct super_block *sup_blk,
		              struct statfs *fs_info, int size)
#endif
{
#if POST_20_KERNEL_F
	int			ret;
#endif
	struct statfs		buf;
	ovlfs_super_info_t	*s_info;

	DPRINTC2("sup_blk 0x%08x, s_dev 0x%x, fs_info 0x%08x\n",
	          (int) sup_blk, sup_blk->s_dev, (int) fs_info);

	ret = 0;

#if PRE_24_KERNEL_F

	if ( size < sizeof(struct statfs) )
	{
		DPRINT1(("OVLFS: statfs structure size mismatch; called size "
		         "is %d, expected %d\n", size, sizeof(struct statfs)));

# if POST_20_KERNEL_F
		return	-EINVAL;
# else
		return;
# endif
	}

#endif

		/* How do we measure available and used blocks without */
		/*  some storage space?  For now, use the available    */
		/*  free memory as available space and calculate an    */
		/*  approximate memory use based on the number of      */
		/*  inodes "owned" by the psuedo filesystem.           */

	buf.f_type = OVLFS_SUPER_MAGIC;
	buf.f_bsize = 1024;
	buf.f_bfree = 0;
	buf.f_bavail = 0;
	buf.f_files = 0;
	buf.f_ffree = 0xFF;	/* TDB: determine the default value. */
	buf.f_namelen = NAME_MAX;


		/* Call the storage system's statfs. */

	if ( ( sup_blk != (struct super_block *) NULL ) &&
	     ( sup_blk->u.generic_sbp != NULL ) &&
	     ( sup_blk->s_magic == OVLFS_SUPER_MAGIC ) )
	{
		s_info = (ovlfs_super_info_t *) sup_blk->u.generic_sbp;

		if ( ( s_info != NULL ) &&
		     ( s_info->stg_ops != NULL ) &&
		     ( s_info->stg_ops->Statfs != NULL ) )
		{
#if POST_22_KERNEL_F
				/* TBD: update Statfs() to return value */
			s_info->stg_ops->Statfs(sup_blk, &buf, sizeof buf);
#else
			s_info->stg_ops->Statfs(sup_blk, &buf, size);
#endif
		}
	}

#if POST_22_KERNEL_F
	memcpy(fs_info, &buf, sizeof(struct statfs));
#elif POST_20_KERNEL_F
	if ( copy_to_user(fs_info, &buf, sizeof(struct statfs)) )
		ret = -EFAULT;
#else
	if ( memcpy_tofs(fs_info, &buf, sizeof(struct statfs)) )
		ret = -EFAULT;
#endif

	DPRINTR2("sup_blk 0x%08x, s_dev 0x%x, fs_info 0x%08x\n",
	          (int) sup_blk, sup_blk->s_dev, (int) fs_info);

#if POST_20_KERNEL_F
	return	ret;
#endif
}



/**
 ** FUNCTION: ovl_fs_read_super
 **
 **  PURPOSE: Perform the read_super operation for the given overlay filesystem
 **           superblock.  Note that the storage operations must be defined for
 **           the superblock, otherwise -EINVAL is returned.
 **/

static struct super_block	*ovl_fs_read_super (struct super_block *sup_blk,
				                    void *options, int silent)
{
	ovlfs_super_info_t	sup_info;
	struct inode		*mnt_pt;
	char			*unused_opts;
	int			rc;

	DPRINTC((KDEBUG_CALL_PREFIX "ovl_fs_read_super\n"));

		/* Increment the use count before doing any real work   */
		/*  so that the module can not be unloaded while in use */

#if MODULE
	MOD_INC_USE_COUNT;
#endif

	START_ALLOC("ovlfs_read_super");

	LOCK_SUPER20(sup_blk);

	sup_blk->s_op = &ovl_super_ops;

	sup_blk->s_magic = OVLFS_SUPER_MAGIC;

	DPRINT5(("OVLFS: device for super block = %d\n", sup_blk->s_dev));


		/* Initialize all pointers to NULL and integers to 0 */

	memset(&sup_info, '\0', sizeof(sup_info));

	sup_info.storage_opts  = OVLFS_STG_DEF_OPTS;
	sup_info.magic_opts    = OVLFS_ALL_MAGIC;
	unused_opts = (char *) NULL;

	rc = ovlfs_parse_opts((char *) options, &sup_info, &unused_opts);

	if ( rc < 0 )
	{
#if MODULE
		MOD_DEC_USE_COUNT;
#endif
		DPRINT1(("OVLFS: ovlfs_read_super: ovlfs_parse_opts returned "
		         "%d\n", rc));

		if ( unused_opts != (char *) NULL )
			FREE(unused_opts);

		UNLOCK_SUPER20(sup_blk);

		STOP_ALLOC("ovlfs_read_super");
		return	(struct super_block *) NULL;
	}

/* TBD: if neither storage nor base roots are available, shouldn't this   */
/*      stop?  Likewise, if either is not a directory, shouldn't an error */
/*      result.  Does it make any sense otherwise?                        */

	fill_in_opt_info(sup_blk, &sup_info);
	sup_blk->u.generic_sbp = (void *) dup_ovlfs_opts(sup_blk, &sup_info);

	if ( sup_blk->u.generic_sbp == NULL )
	{
#if MODULE
		MOD_DEC_USE_COUNT;
#endif

		DPRINT1(("OVLFS: ovlfs_read_super: unable to allocate memory "
		         "needed for the super block\n"));

		if ( unused_opts != (char *) NULL )
			FREE(unused_opts);

		cleanup_super(sup_blk);
		UNLOCK_SUPER20(sup_blk);

		STOP_ALLOC("ovlfs_read_super");
		return	(struct super_block *) NULL;
	}

		/* Read and setup any storage information associated */
		/*  with the super block.                            */

	rc = do_read_super_op(sup_blk, unused_opts);

	if ( rc < 0 )
	{
#if MODULE
		MOD_DEC_USE_COUNT;
#endif

		DPRINT1(("OVLFS: ovlfs_read_super: error reading storage "
		         "information for the super block\n"));

			/* Need to clean up resources "allocated" above, */
			/*  unless the super block will be "put"...      */


		if ( unused_opts != (char *) NULL )
			FREE(unused_opts);

		cleanup_super(sup_blk);
		UNLOCK_SUPER20(sup_blk);

		STOP_ALLOC("ovlfs_read_super");
		return	(struct super_block *) NULL;
	}

		/* Free the string of options that were passed to the  */
		/*  storage method above since it is no longer needed. */

	if ( unused_opts != (char *) NULL )
		FREE(unused_opts);


		/* Get the root inode */

#if POST_20_KERNEL_F
	mnt_pt = iget(sup_blk, OVLFS_ROOT_INO);
#else
	mnt_pt = __iget(sup_blk, OVLFS_ROOT_INO, 0);
#endif

	if ( mnt_pt != INODE_NULL )
	{
		DPRINT2(("OVLFS: mount point: dev %d, inode %ld\n",
		         mnt_pt->i_dev, mnt_pt->i_ino));

#if POST_20_KERNEL_F
# if POST_22_KERNEL_F
		sup_blk->s_root = d_alloc_root(mnt_pt);
# else
		sup_blk->s_root = d_alloc_root(mnt_pt, NULL);
# endif

		if ( sup_blk->s_root == NULL )
		{
			IPUT(mnt_pt);
			mnt_pt = INODE_NULL;
		}
#else
		sup_blk->s_mounted = mnt_pt;
#endif

	}
	else
	{
		DPRINT2(("OVLFS: unable to obtain inode for the mount pt.\n"));
	}

		/* Clean up on failure. */

	if ( mnt_pt == INODE_NULL )
	{
#if MODULE
		MOD_DEC_USE_COUNT;
#endif

			/* Need to clean up resources "allocated" above, */
			/*  unless the super block will be "put"...      */

		do_release_super_op(sup_blk);
		cleanup_super(sup_blk);
		UNLOCK_SUPER20(sup_blk);

		STOP_ALLOC("ovlfs_read_super");
		return	(struct super_block *) NULL;
	}


		/* Use the system's page size by default in order to */
		/*  minimize the number of blocks per page.          */

	sup_blk->s_blocksize      = PAGE_SIZE;
	sup_blk->s_blocksize_bits = PAGE_SHIFT;

	UNLOCK_SUPER20(sup_blk);

	DPRINTC(("OVLFS: ovl_fs_read_super returning\n"));

	return	sup_blk;
}



			/**************************/
			/** OVLFS ADMINISTRATION **/
			/**************************/



/**
 ** FUNCTION: init_ovl_fs
 **
 **  PURPOSE: Register the overlay filesystem with the kernel.
 **/

#ifdef DECLARE_FSTYPE
static DECLARE_FSTYPE(ovl_fs, "ovl", ovl_fs_read_super, 0);
#else
static struct file_system_type	ovl_fs = { ovl_fs_read_super, "ovl", 0,
                                           THIS_MODULE, NULL, };
#endif

 /* ovl_fs2: not yet used -- will be for real devices: */
#if TBD_FS2
static struct file_system_type	ovl_fs2 = { ovl_fs_read_super, "ovl2", 1,
                                            THIS_MODULE, NULL, };
#endif

int	init_ovl_fs ()
{
	int	err_code;

	err_code = register_filesystem(&ovl_fs);

#if TBD_FS2
	err_code = register_filesystem(&ovl_fs2);
#endif

	return	err_code;
}



			/**** MODULE INTERFACE ****/

#ifdef MODULE

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arthur Naseef");
MODULE_DESCRIPTION("Overlay Filesystem");

/**
 ** Symbol Table: define symbols (versioned symbols) that are exported from or
 **               needed by this module.
 **/

/* #ifdef EXPORT_SYMBOL	-- would be nice. For want of a better solution! */

#if POST_20_KERNEL_F

EXPORT_SYMBOL(init_ovl_fs);
EXPORT_SYMBOL(ovlfs_end_debug);
EXPORT_SYMBOL(ovlfs_dprintf);
EXPORT_SYMBOL(ovlfs_resolve_inode);
# if DEBUG_BUSY_OVLFS
EXPORT_SYMBOL(ovlfs_debug_inodes);
# endif
EXPORT_SYMBOL(ovlfs_copy_file);
EXPORT_SYMBOL(ovlfs_ino_find_dirent);

#else

static struct symbol_table	ovlfs_symtab = {
# include <linux/symtab_begin.h>
	X(init_ovl_fs),
	X(ovlfs_end_debug),
	X(ovlfs_dprintf),
	X(ovlfs_resolve_inode),
# if DEBUG_BUSY_OVLFS
	X(ovlfs_debug_inodes),
# endif
	X(ovlfs_copy_file),
/*	X(ovlfs_prepare_inode), */
	X(ovlfs_ino_find_dirent),
# include <linux/symtab_end.h>
} ;

#endif


MODULE_PARM(ovlfs_debug_level, "1-1i");


int	init_module ()
{
	int	err_code;

	DPRINTC2("starting");

#if KDEBUG > 1 || OVLFS_BANNER
	printk("starting ovl fs V%s module (compiled %s)\n",
	       macro2str(OVLFS_RELEASE), macro2str(DATE_STAMP));
#endif
	START_ALLOC("init_module");

	err_code = init_ovl_fs();

	if ( err_code >= 0 )
	{
#ifndef EXPORT_SYMBOL
			/* Old method - explicitly ask the kernel to */
			/*  register the symbols table additions.    */

		register_symtab(&ovlfs_symtab);
#endif
	}

	DPRINTR2("done; ret = %d\n", err_code);

	return	err_code;
}

void	cleanup_module()
{
	STOP_ALLOC("cleanup_module");

#if KDEBUG > 1 || OVLFS_BANNER
	printk("removing ovl fs V%s module (compiled %s)\n",
	       macro2str(OVLFS_RELEASE), macro2str(DATE_STAMP));
#endif

	unregister_filesystem(&ovl_fs);
}

#endif
