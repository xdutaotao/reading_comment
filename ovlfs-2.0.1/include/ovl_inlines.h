/******************************************************************************
 ******************************************************************************
 **
 ** FILE: ovl_inlines.h
 **
 ** DESCRIPTION: This file contains inline function definitions used by more
 **              than one source file within the overlay filesystem source.
 **
 ** NOTES:
 **	- This is not the best programming practice because defining functions
 **	  in header files makes them hard to track and maintain.  However, all
 **	  of these functions are static inline functions, so they are closer
 **	  to macros than actual functions.
 **
 **	- The value of placing these static inline functions here is that they
 **	  may be used across multiple source files while maintaining a single
 **	  version of them and keeping them static (i.e. non-exported).
 **
 **
 ** REVISION HISTORY:
 **
 ** DATE	AUTHOR		DESCRIPTION
 ** ==========	===============	==============================================
 ** 01/17/1999	ARTHUR N.	Added to source code control.
 ** 02/02/1999	ARTHUR N.	Added the do_release_super_op() function and
 **				 added the option string to the Init()
 **				 function call.
 ** 02/06/1999	ARTHUR N.	Added the check_dir_update() function.  Also,
 **				 commented out inode locking.
 ** 02/06/1999	ARTHUR N.	Removed the check_dir_update() function and
 **				 modified do_map_lookup(), do_add_inode_op(),
 **				 do_lookup_op(), and do_map_inode() to support
 **				 the removal of serving inodes from the Storage
 **				 System interface.
 ** 02/15/2003	ARTHUR N.	Pre-release of port to 2.4.
 ** 03/08/2003	ARTHUR N.	Added do_get_mapping_op() and
 **				 is_base_or_stg_root_p().
 ** 03/11/2003	ARTHUR N.	Added do_delete_dirent_op().
 ** 03/11/2003	ARTHUR N.	Added flags argument to add_inode storage op.
 ** 03/26/2003	ARTHUR N.	Changed the Read_dir() storage system entry
 **				 point.
 **
 ******************************************************************************
 ******************************************************************************
 **/

#ifndef __ovl_inlines_STDC_INCLUDE
#define __ovl_inlines_STDC_INCLUDE	1

# ident "@(#)ovl_inlines.h	1.9	[03/07/27 22:20:36]"

# define OVL_INLINE	static inline

# define PARNOID_CHECKS	1

/******************************************************************************
 ******************************************************************************
 **
 **			GENERAL PURPOSE OPERATIONS
 **
 ******************************************************************************
 ******************************************************************************
 **/


/**
 ** FUNCTION: dup_string
 **/

OVL_INLINE char	*dup_string (const char *str)
{
	char	*result;

	if ( str != (char *) NULL )
	{
		result = (char *) MALLOC(strlen(str) + 1);

		if ( result != (char *) NULL )
			strcpy(result, str);
	}
	else
		result = NULL;

	return	result;
}



/**
 ** FUNCTION: dup_buffer
 **/

OVL_INLINE char	*dup_buffer (const char *buf, int len)
{
	char	*result;

	if ( ( buf != (char *) NULL ) && ( len > 0 ) )
	{
		result = (char *) MALLOC(len);

		if ( result != (char *) NULL )
			memcpy(result, buf, len);
	}
	else
		result = NULL;

	return	result;
}


/**
 ** FUNCTION: ovlfs_use_inode
 **
 **  PURPOSE: This function is called when an inode is going to be used and
 **           it is important to make certain that the inode is not locked.
 **/

OVL_INLINE int	ovlfs_use_inode (struct inode *inode)
{
# if PARANOID_CHECKS
	if ( inode == (struct inode *) NULL )
	{
		DPRINT1(("OVLFS: ovlfs_use_inode called with NULL inode!\n"));
		return	-1;
	}
# endif

#ifdef I_LOCK
	if ( inode->i_state & I_LOCK )
#else
	if ( inode->i_lock )
#endif
		return	ovlfs_wait_on_inode(inode);

	return  0;
}



/**
 ** FUNCTION: ovlfs_release_inode
 **
 **  PURPOSE: Undo the work done by ovlfs_use_inode() once the inode is
 **           not being used anymore.  This currently doesn't do anything.
 **/

OVL_INLINE void	ovlfs_release_inode (struct inode *inode)
{
}



/**
 ** FUNCTION: is_base_or_stg_root_p
 **
 **  PURPOSE: Given a device and inode number pair, check whether they match
 **           the root of the base or storage filesystem for the given ovlfs
 **           super block.
 **/

OVL_INLINE int	is_base_or_stg_root_p (struct super_block *sb, kdev_t dev,
		                       ino_t inum)
{
	ovlfs_super_info_t	*s_info;
	struct inode		*base_i;
	struct inode		*stg_i;
	int			rc;

	s_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	rc = FALSE;

#if USE_DENTRY_F
	if ( s_info->root_ent != NULL )
		base_i = s_info->root_ent->d_inode;
	else
		base_i = NULL;
		
	if ( s_info->storage_ent != NULL )
		stg_i = s_info->storage_ent->d_inode;
	else
		stg_i = NULL;
#else
	base_i = s_info->root_ent;
	stg_i  = s_info->storage_ent;
#endif

	if ( ( base_i != NULL ) && ( base_i->i_dev == dev ) &&
	     ( base_i->i_ino == inum ) )
	{
		rc = TRUE;
	}
	else if ( ( stg_i != NULL ) && ( stg_i->i_dev == dev ) &&
	          ( stg_i->i_ino == inum ) )
	{
		rc = TRUE;
	}

	return	rc;
}



/******************************************************************************
 ******************************************************************************
 **
 **			STORAGE OPERATIONS
 **
 ******************************************************************************
 ******************************************************************************
 **/

/**
 ** FUNCTION: do_read_super_op
 **
 **  PURPOSE: Given the superblock of a pseudo filesystem, perform the
 **           Init storage operation on the superblock.
 **/

OVL_INLINE int	do_read_super_op (struct super_block *sb, char *opt_str)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( sb == (struct super_block *) NULL ) ||
	     ( sb->s_magic != OVLFS_SUPER_MAGIC ) )
	{
		DPRINT1(("OVLFS: do_read_super_op called with invalid sb!\n"));
		return	-EINVAL;
	}
# endif

	sup_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	if ( ( sup_info == NULL ) ||
	     ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Init == NULL ) )
	{
		DPRINT1(("OVLFS: do_read_super_op: storage ops or Init method "
		         "NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Init(sb, opt_str);
}



/**
 ** FUNCTION: do_release_super_op
 **
 **  PURPOSE: Given the superblock of a pseudo filesystem, perform the
 **           Release storage operation on the superblock.
 **/

OVL_INLINE int	do_release_super_op (struct super_block *sb)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( sb == (struct super_block *) NULL ) ||
	     ( sb->s_magic != OVLFS_SUPER_MAGIC ) )
	{
		DPRINT1(("OVLFS: do_release_super_op called with invalid "
		         "sb!\n"));
		return	-EINVAL;
	}
# endif

	sup_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	if ( ( sup_info == NULL ) ||
	     ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Release == NULL ) )
	{
		DPRINT1(("OVLFS: do_release_super_op: storage ops or Release "
		         "method NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Release(sb);
}



/**
 ** FUNCTION: do_unlink_op
 **
 **  PURPOSE: Perform the Unlink storage operation for the given inode from a
 **           pseudo filesystem.
 **/

OVL_INLINE int	do_unlink_op (struct inode *inode, const char *fname, int len)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( inode == (struct inode *) NULL ) ||
	     ( ! is_ovlfs_inode(inode) ) )
	{
		DPRINT1(("OVLFS: do_read_super_op called with invalid "
		         "inode!\n"));
		return	-EINVAL;
	}
# endif

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Unlink == NULL ) )
	{
		DPRINT1(("OVLFS: do_unlink_op: storage ops or Unlink method "
		         "NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Unlink(inode, fname, len);
}



/**
 ** FUNCTION: do_delete_dirent_op
 **
 **  PURPOSE: Perform the Delete_dirent storage operation for the given inode
 **           from a pseudo filesystem.
 **/

OVL_INLINE int	do_delete_dirent_op (struct inode *inode, const char *fname,
		                     int len)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( inode == (struct inode *) NULL ) ||
	     ( ! is_ovlfs_inode(inode) ) )
	{
		DPRINT1(("OVLFS: do_read_super_op called with invalid "
		         "inode!\n"));
		return	-EINVAL;
	}
# endif

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Delete_dirent == NULL ) )
	{
		DPRINT1(("OVLFS: do_delete_dirent_op: storage ops or "
		         "Delete_dirent method NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Delete_dirent(inode, fname, len);
}



/**
 ** FUNCTION: do_num_dirent_op
 **
 **  PURPOSE: Perform the Num_dirent storage operation on the specified inode.
 **/

OVL_INLINE int	do_num_dirent_op (struct inode *inode, int inc_rm)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: do_num_dirent_op called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode->i_sb == NULL ) || ( inode->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Num_dirent == NULL ) )
	{
		DPRINT1(("OVLFS: do_num_dirent_op: storage ops or Num_dirent "
		         "method NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Num_dirent(inode, inc_rm);
}



/**
 ** FUNCTION: do_lookup_op
 **
 **  PURPOSE: Perform the Lookup storage operation on the specified inode.
 **/

OVL_INLINE int	do_lookup_op (struct inode *dir, const char *name, int len,
		              _ulong *result, int *r_flags)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( dir == (struct inode *) NULL) || ( ! is_ovlfs_inode(dir) ) ||
	    ( result == (_ulong *) NULL ))
	{
		DPRINT1(("OVLFS: do_lookup_op called with NULL argument!\n"));
		return	-ENOTDIR;
	}
# endif

	if ( ( dir->i_sb == NULL ) || ( dir->i_sb->u.generic_sbp == NULL ) )
		return	-ENOTDIR;

	sup_info = (ovlfs_super_info_t *) dir->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Lookup == NULL ) )
	{
		DPRINT1(("OVLFS: do_lookup_op: storage ops or Lookup method "
		         "NULL!\n"));

		return	-ENOTDIR;
	}

	return	sup_info->stg_ops->Lookup(dir, name, len, result, r_flags);
}



/**
 ** FUNCTION: do_read_dir_op
 **
 **  PURPOSE: Perform the Read_dir storage operation on the specified inode.
 **/

OVL_INLINE int	do_read_dir_op (struct inode *dir, loff_t *pos,
		                ovlfs_dir_info_t **result)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( dir == (struct inode *) NULL) || ( ! is_ovlfs_inode(dir) ) ||
	    ( result == (ovlfs_dir_info_t **) NULL ))
	{
		DPRINT1(("OVLFS: do_read_dir_op called with NULL arg!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( dir->i_sb == NULL ) || ( dir->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) dir->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Read_dir == NULL ) )
	{
		DPRINT1(("OVLFS: do_read_dir_op: storage ops or Read_dir "
		         "method NULL!\n"));

		return	-ENOTDIR;
	}

	return	sup_info->stg_ops->Read_dir(dir, pos, result);
}



/**
 ** FUNCTION: do_add_dirent
 **
 **  PURPOSE: Perform the Add_dirent storage operation on the specified inode.
 **/

OVL_INLINE int	do_add_dirent (struct inode *inode, const char *name, int len,
			       _ulong inum)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: do_add_dirent called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode->i_sb == NULL ) || ( inode->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Add_dirent == NULL ) )
	{
		DPRINT1(("OVLFS: do_add_dirent: storage ops or Add_dirent "
		         "method NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Add_dirent(inode, name, len, inum);
}



/**
 ** FUNCTION: has_rename_op
 **
 **  PURPOSE: Determine whether the given inode has a Rename() operation or
 **           not.  The pseudo filesystem will work around a missing Rename().
 **/

OVL_INLINE int	has_rename_op (struct inode *inode)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: has_rename_op called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode->i_sb == NULL ) || ( inode->i_sb->u.generic_sbp == NULL ) )
		return	0;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Rename == NULL ) )
		return	0;

	return	1;
}



/**
 ** FUNCTION: do_rename_op
 **
 **  PURPOSE: Perform the Rename storage operation on the specified inode in
 **           the pseudo filesystem.  This rename is only used when the new
 **           and old file names are in the SAME DIRECTORY.
 **/

OVL_INLINE int	do_rename_op (struct inode *inode, const char *oname, int olen,
			      const char *nname, int nlen)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: do_num_dirent_op called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode->i_sb == NULL ) || ( inode->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Rename == NULL ) )
	{
		DPRINT1(("OVLFS: do_rename_op: storage ops or Rename method "
		         "NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Rename(inode, oname, olen, nname, nlen);
}



/**
 ** FUNCTION: do_add_inode_op
 **
 **  PURPOSE: Perform the Add_inode storage operation.
 **/

OVL_INLINE int	do_add_inode_op (struct super_block *sb, _ulong parent_inum,
		                 const char *name, int namelen, _ulong *result,
		                 int flags)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( sb == (struct super_block *) NULL) ||
	     ( sb->s_magic != OVLFS_SUPER_MAGIC ) ||
	     ( result == (_ulong *) NULL ) )
	{
		DPRINT1(("OVLFS: do_add_inode_op called with NULL arg!\n"));
		return	-EINVAL;
	}
# endif

	if ( sb->u.generic_sbp == NULL )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Add_inode == NULL ) )
	{
		DPRINT1(("OVLFS: do_update_inode: storage ops or Add_inode "
		         "method NULL!\n"));

			/* Just tell the user there is no space */

		return	-ENOSPC;
	}

	return	sup_info->stg_ops->Add_inode(sb, parent_inum, name, namelen,
		                             result, flags);
}



/**
 ** FUNCTION: do_read_inode_op
 **
 **  PURPOSE: Perform the Read_inode storage operation.
 **/

OVL_INLINE int	do_read_inode_op (struct inode *inode, int *r_valid_f)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: do_read_inode_op called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode == INODE_NULL ) ||
	     ( inode->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Read_inode == NULL ) )
	{
		DPRINT1(("OVLFS: do_read_inode_op: storage ops or Read_inode "
		         "method NULL!\n"));

		return	-ENOENT;
	}

	return	sup_info->stg_ops->Read_inode(inode, r_valid_f);
}



/**
 ** FUNCTION: do_update_inode
 **
 **  PURPOSE: Perform the Update Inode storage operation on the specified inode
 **           in the pseudo filesystem.
 **/

OVL_INLINE int	do_update_inode (struct inode *inode)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if (( inode == (struct inode *) NULL) || ( ! is_ovlfs_inode(inode) ))
	{
		DPRINT1(("OVLFS: do_update_inode called with NULL inode!\n"));
		return	-EINVAL;
	}
# endif

	if ( ( inode->i_sb == NULL ) || ( inode->i_sb->u.generic_sbp == NULL ) )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) inode->i_sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Update_inode == NULL ) )
	{
		DPRINT1(("OVLFS: do_update_inode: storage ops or Update_inode "
		         "method NULL!\n"));

		return	-EINVAL;
	}

	return	sup_info->stg_ops->Update_inode(inode);
}



/**
 ** FUNCTION: do_map_inode
 **
 **  PURPOSE: Perform the Map_inode storage operation for the filesystem
 **           referenced by the given super block.
 **/

OVL_INLINE int	do_map_inode (struct super_block *sb1, _ulong inum1,
		              kdev_t dev2, _ulong inum2, char which)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( sb1->s_magic != OVLFS_SUPER_MAGIC )
	{
		DPRINT1(("OVLFS: do_map_inode called with invalid super"
		         "block!\n"));

		return	-EINVAL;
	}
# endif

		/* Update the information directly in the inode, if we can */

	sup_info = (ovlfs_super_info_t *) sb1->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Map_inode == NULL ) )
	{
		DPRINT2(("OVLFS: do_map_inode: storage ops or Map_inode method"
		         " NULL!\n"));


			/* No map support; this is ok, but not wise */

		return	0;
	}

	return	sup_info->stg_ops->Map_inode(sb1, inum1, dev2, inum2, which);
}



/**
 ** FUNCTION: do_get_mapping_op
 **
 **  PURPOSE: Perform the Map_inode storage operation for the filesystem
 **           referenced by the given super block.
 **/

OVL_INLINE int	do_get_mapping_op (struct super_block *sb, _ulong inum,
		                   char which, kdev_t *r_dev, _ulong *r_ino)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( sb->s_magic != OVLFS_SUPER_MAGIC )
	{
		DPRINT1(("OVLFS: %s called with invalid super block!\n",
		         __FUNCTION__));

		return	-EINVAL;
	}
# endif

	sup_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Get_mapping == NULL ) )
	{
		DPRINT2(("OVLFS: %s: storage ops or Map_inode method NULL!\n",
		         __FUNCTION__));


			/* No map support; this is ok, but not wise */

		return	0;
	}

	return	sup_info->stg_ops->Get_mapping(sb, inum, which, r_dev, r_ino);
}



/**
 ** FUNCTION: do_map_lookup
 **
 **  PURPOSE: Perform the Map_lookup storage operation for the filesystem
 **           referenced by the given super block.
 **/

OVL_INLINE int	do_map_lookup (struct super_block *sb, kdev_t dev, _ulong inum,
		               _ulong *result, char which)
{
	ovlfs_super_info_t	*sup_info;

# if PARANOID_CHECKS
	if ( ( sb == (struct super_block *) NULL) ||
	     ( sb->s_magic != OVLFS_SUPER_MAGIC ) ||
	     ( result == (_ulong *) NULL ) )
	{
		DPRINT1(("OVLFS: do_map_lookup called with NULL arg!\n"));
		return	-EINVAL;
	}
# endif

	if ( sb->u.generic_sbp == NULL )
		return	-EINVAL;

	sup_info = (ovlfs_super_info_t *) sb->u.generic_sbp;

	if ( ( sup_info->stg_ops == NULL ) ||
	     ( sup_info->stg_ops->Map_lookup == NULL ) )
	{
		DPRINT1(("OVLFS: do_map_lookup: storage ops or Map_lookup "
		         "method NULL!\n"));

		return	-ENOENT;
	}

	return	sup_info->stg_ops->Map_lookup(sb, dev, inum, result, which);
}



/**
 ** FUNCTION: ovlfs_check_kernel_stack
 **
 **  PURPOSE: Check the kernel stack for the current process to make sure it
 **           is valid.  This is helpful in debugging stack corruption
 **           problems.
 **/

OVL_INLINE
int	ovlfs_check_kernel_stack (char *fname, int lineno, const char *msg)
{
	static int	corrupt_msg_cnt = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0)

	unsigned long	*ptr;

	ptr = (unsigned long *)current->kernel_stack_page;

	if ( ptr[0] != STACK_MAGIC )
	{
		if ( corrupt_msg_cnt < 10 )
		{
			printk("OVLFS: kernel stack corrupt at %s, %d: %s\n",
			       fname, lineno, msg);

			corrupt_msg_cnt++;

			return	1;
		}
	}

#else
	char	*ptr;

	ptr = (char *) current;

	if ( (char *) &ptr < ( ptr + sizeof (struct task_struct) ) )
	{
		if ( corrupt_msg_cnt < 10 )
		{
			printk("OVLFS: kernel stack corrupt at %s, %d: %s\n",
			       fname, lineno, msg);

			corrupt_msg_cnt++;

			return	1;
		}
	}

#endif

	return	0;
}

#endif
/* matches #ifndef __ovl_inlines_STDC_INCLUDE */
